<?php
/**
 * Created by Adam Jakab.
 * Date: 20/06/16
 * Time: 14.16
 */

use MultiSmtp\ConfigNode;

class MultiSmtpMailSystem implements \MailSystemInterface {
  /**
   * Concatenate and wrap the e-mail body for either
   * plain-text or HTML emails.
   *
   * @param array $message
   *   A message array, as described in hook_mail_alter().
   *
   * @return string
   *   The formatted $message.
   *
   * @throws \Exception
   */
  public function format(array $message) {
    $smtpConfig = ConfigNode::getConfigNodeByEmail($message["from"]);
    if (variable_get('multismtp_debugging', MULTISMTP_LOGGING_ERRORS)
        == MULTISMTP_LOGGING_ALL
    ) {
      //dpm($smtpConfig, "SMTP-CONFIG(FORMAT)");
      dpm($message, "MESSAGE(FORMAT)");
    }

    // Fix body
    if (is_array($message['body'])) {
      $message['body'] = implode("\n\n", $message['body']);
    }

    // Text or Html
    if ($this->isHtmlMail($smtpConfig, $message)) {
      $message['body'] = nl2br($message['body']);
    }
    else {
      $message['body'] = drupal_html_to_text($message['body']);
      $message['body'] = drupal_wrap_mail($message['body']);
    }

    return $message;
  }

  /**
   * Send the e-mail message.
   *
   * @param array $message
   *   A message array, as described in hook_mail_alter().
   *
   * @return bool
   *   TRUE if the mail was successfully accepted, otherwise FALSE.
   *
   * @throws \Exception
   */
  public function mail(array $message) {
    if (variable_get('multismtp_queue', FALSE)
        && (!isset($message['params']['skip_queue'])
            || !$message['params']['skip_queue'])
    ) {
      smtp_send_queue($message);
      multismtp_debug(
        'multismtp', 'Queueing mail from: @from for: @to',
        [
          '@to' => $message['to'],
          '@from' => $message['from']
        ]
      );
      return TRUE;
    }
    else {
      return $this->mailWithoutQueue($message);
    }
  }

  /**
   * Send the message away.
   *
   * @param array $message
   *
   * @return bool
   */
  protected function mailWithoutQueue(array $message) {
    $smtpConfig = ConfigNode::getConfigNodeByEmail($message["from"]);
    if (variable_get('multismtp_debugging', MULTISMTP_LOGGING_ERRORS)
        == MULTISMTP_LOGGING_ALL
    ) {
      //dpm($smtpConfig, "SMTP-CONFIG(MAIL)");
      dpm($message, "MESSAGE(MAIL)");
    }

    $smtp_server = ConfigNode::getConfigNodeValue($smtpConfig, 'field_smtp_server', FALSE);
    $smtp_port = ConfigNode::getConfigNodeValue($smtpConfig, 'field_smtp_port', FALSE);
    $smtp_security = ConfigNode::getConfigNodeValue($smtpConfig, 'field_smtp_protocol', 'none');
    $smtp_login = ConfigNode::getConfigNodeValue($smtpConfig, 'field_smtp_login', '');
    $smtp_password = ConfigNode::getConfigNodeValue($smtpConfig, 'field_smtp_password', '', 'password_field');
    if ($smtp_password) {
      $smtp_password = password_field_decrypt($smtp_password);
    }

    $from = $this->convertFromStringToArray($message['from']);
    $to = $this->convertFromStringToArray($message['to']);
    $body = $message['body'];
    $headers = $message['headers'];
    $subject = $message['subject'];

    //@todo: force sender name
    /*
    if ($fromName = trim(ConfigNode::getConfigNodeValue($smtpConfig, 'field_smtp_name', ''))) {
      $message["from"] = $fromName . ' <' . $message["from"] . '>';
    }*/

    //$sender = (isset($message["headers"]["Sender"]) ? $message["headers"]["Sender"] : false);
    //$returnPath = (isset($message["headers"]["Return-Path"]) ? $message["headers"]["Return-Path"] : false);
    $sender = ConfigNode::getConfigNodeValue($smtpConfig, 'field_smtp_email', FALSE);
    $returnPath = ConfigNode::getConfigNodeValue($smtpConfig, 'field_smtp_email', FALSE);

    $XMailer = (isset($message["headers"]["X-Mailer"]) ? $message["headers"]["X-Mailer"] : 'Drupal')
               . ' (multismtp)';

    $CTA = $this->convertContentTypeStringToArray(isset($message["headers"]["Content-Type"]) ? $message["headers"]["Content-Type"] : '');
    dpm($CTA, "CTA");

    // The transport
    $transport = \Swift_SmtpTransport::newInstance($smtp_server, $smtp_port);
    $transport->setUsername($smtp_login);
    $transport->setPassword($smtp_password);
    if ($smtp_security != 'none') {
      $transport->setEncryption($smtp_security);
    }

    // The message
    $swiftMessage = \Swift_Message::newInstance($subject);
    $swiftMessage->setCharset($CTA["charset"]);
    $swiftMessage->setContentType($CTA["content-type"]);
    $swiftMessage->setDelSp(isset($CTA["delsp"]) && $CTA["delsp"] == "yes");
    $swiftMessage->setFormat($CTA["format"]);
    $swiftMessage->setBody($body);
    $swiftMessage->setFrom($from);
    $swiftMessage->setTo($to);
    if ($sender) {
      $swiftMessage->setSender($sender);
    }
    if ($returnPath) {
      $swiftMessage->setReturnPath($returnPath);
    }
    if ($XMailer) {
      $headers = $swiftMessage->getHeaders();
      $headers->addTextHeader('X-Mailer', $XMailer);
    }

    // Send mail
    $failedRecipients = [];
    $mailer = \Swift_Mailer::newInstance($transport);
    $successCount = $mailer->send($swiftMessage, $failedRecipients);
    $result = $successCount != 0;

    if (count($failedRecipients)) {
      multismtp_debug('multismtp', 'Error sending mail to %to', ['%to' => json_encode($failedRecipients)], WATCHDOG_ERROR);
    }

    multismtp_debug('multismtp', 'Sending: %message.', ['%message' => json_encode($message)], WATCHDOG_DEBUG);
    return $result;
  }


  /**
   * Decide if the mail is to be sent as html or not.
   *
   * @param \stdClass $smtpConfig
   * @param array $message
   *
   * @return bool
   */
  protected function isHtmlMail($smtpConfig, &$message) {
    $answer = FALSE;
    if (ConfigNode::getConfigNodeValue($smtpConfig, 'field_smtp_allow_html', '0')
        == "1"
    ) {
      // ok the configuration does allow HTML mail
      // check if message is in html format;
      if (isset($message["headers"]["Content-Type"])) {
        $CTA = $this->convertContentTypeStringToArray($message["headers"]["Content-Type"]);
        if (isset($CTA["content-type"]) && $CTA["content-type"] == "text/html") {
          $answer = TRUE;
        }
      }
    }
    return $answer;
  }

  /**
   * Extract pieces from content type string such as:
   * 'text/plain; charset=UTF-8; format=flowed; delsp=yes'
   * to keyed array
   *
   * @param string $contentTypeString
   *
   * @return array
   */
  protected function convertContentTypeStringToArray($contentTypeString) {
    $answer = [];
    $allowedContentTypes = ["text/plain", "text/html"];
    $defaultContentType = $allowedContentTypes[0];

    // Set the defaults
    $answer["content-type"] = $defaultContentType;
    $answer["charset"] = 'UTF8-8';
    $answer["format"] = 'flowed';
    $answer["delsp"] = 'yes';

    $parts = explode(";", $contentTypeString);
    foreach ($parts as $part) {
      $part = trim($part);
      if (in_array($part, $allowedContentTypes)) {
        $answer["content-type"] = $part;
      }
      else {
        //something like 'charset=UTF-8' | 'delsp=yes'
        if (strpos($part, "=")) {
          $partArray = explode("=", $part);
          $partKey = trim($partArray[0]);
          $partVal = trim($partArray[1]);
          $answer[$partKey] = $partVal;
        }
      }
    }
    return $answer;
  }

  /**
   * Converts from/to strings like:
   *  sender@mail.com             =>      [sender@mail.com]
   *  s1@mail.com,s2@mail.com     =>      [s1@mail.com, s2@mail.com]
   *  "Sender 1" <s1@mail.com>    =>      [s1@mail.com => "Sender 1"]
   *
   * Purposely, does NOT check if email address is valid (that should be done later)
   *
   * @param string $fromToString
   *
   * @return array
   */
  protected function convertFromStringToArray($fromToString) {
    $answer = [];
    $mails = [];
    if (strpos($fromToString, ",")) {
      $mails = explode(",", $fromToString);
    }
    else {
      $mails[] = trim($fromToString);
    }
    foreach ($mails as $mail) {
      if (preg_match('/^([^<]*)<([^>]*)>.*$/', $mail, $m)) {
        $emailAddress = trim($m[2]);
        $emailName = trim($m[1]);
        $answer[$emailAddress] = $emailName;
      }
      else {
        $answer[] = $mail;
      }
    }
    return $answer;
  }
}