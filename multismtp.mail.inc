<?php

use MultiSmtp\ConfigNode;

/**
 * Created by Adam Jakab.
 * Date: 20/06/16
 * Time: 14.16
 */
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
    $smtpConfig = \MultiSmtp\ConfigNode::getConfigNodeByEmail($message["from"]);
    if (variable_get('multismtp_debugging', MULTISMTP_LOGGING_ERRORS)
        == MULTISMTP_LOGGING_ALL
    ) {
      dpm($smtpConfig, "SMTP-CONFIG(FORMAT)");
    }

    //force sender name
    /*
    if ($fromName = trim(ConfigNode::getConfigNodeValue($smtpConfig, 'field_smtp_name', ''))) {
      $message["from"] = $fromName . ' <' . $message["from"] . '>';
    }*/

    // Text or Html
    if (is_array($message['body'])) {
      $message['body'] = implode("\n\n", $message['body']);
    }
    if (ConfigNode::getConfigNodeValue($smtpConfig, 'field_smtp_allow_html', '0')
        == "1"
    ) {
      $message['body'] = nl2br($message['body']);
    }
    else {
      $message['body'] = drupal_html_to_text($message['body']);
      $message['body'] = drupal_wrap_mail($message['body']);
    }

    //multismtp_debug('multismtp', 'Formatted: %message.', ['%message' => json_encode($message)], WATCHDOG_DEBUG);
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
    $smtpConfig = \MultiSmtp\ConfigNode::getConfigNodeByEmail($message["from"]);
    if (variable_get('multismtp_debugging', MULTISMTP_LOGGING_ERRORS)
        == MULTISMTP_LOGGING_ALL
    ) {
      dpm($smtpConfig, "SMTP-CONFIG(MAIL)");
    }

    $from = $this->convertFromStringToArray($message['from']);
    $to = $this->convertFromStringToArray($message['to']);
    $body = $message['body'];
    $headers = $message['headers'];
    $subject = $message['subject'];

    $smtp_server = ConfigNode::getConfigNodeValue($smtpConfig, 'field_smtp_server', FALSE);
    $smtp_port = ConfigNode::getConfigNodeValue($smtpConfig, 'field_smtp_port', FALSE);
    $smtp_security = ConfigNode::getConfigNodeValue($smtpConfig, 'field_smtp_protocol', 'none');
    $smtp_login = ConfigNode::getConfigNodeValue($smtpConfig, 'field_smtp_login', '');
    $smtp_password = ConfigNode::getConfigNodeValue($smtpConfig, 'field_smtp_password', '', 'password_field');
    if ($smtp_password) {
      $smtp_password = password_field_decrypt($smtp_password);
    }
    $content_type = 'text/plain';
    if (ConfigNode::getConfigNodeValue($smtpConfig, 'field_smtp_allow_html', '0') == "1") {
      $content_type = 'text/html';
    }

    $transport = \Swift_SmtpTransport::newInstance($smtp_server, $smtp_port);
    $transport->setUsername($smtp_login);
    $transport->setPassword($smtp_password);
    if ($smtp_security != 'none') {
      $transport->setEncryption($smtp_security);
    }

    $swiftMessage = \Swift_Message::newInstance($subject);
    $swiftMessage->setCharset('UTF-8');
    $swiftMessage->setContentType($content_type);
    $swiftMessage->setDelSp(TRUE);
    $swiftMessage->setBody($body);
    $swiftMessage->setFrom($from);
    $swiftMessage->setTo($to);

    $failedRecipients = [];
    $mailer = \Swift_Mailer::newInstance($transport);
    $successCount = $mailer->send($swiftMessage, $failedRecipients);
    $result = $successCount != 0;

    if(count($failedRecipients)) {
      multismtp_debug('multismtp', 'Error sending mail to %to', ['%to' => json_encode($failedRecipients)], WATCHDOG_ERROR);
    }

    multismtp_debug('multismtp', 'Sending: %message.', ['%message' => json_encode($message)], WATCHDOG_DEBUG);
    return $result;
  }

  /**
   * Converts from/to strings like:
   *  sender@mail.com             =>      [sender@mail.com]
   *  s1@mail.com,s2@mail.com     =>      [s1@mail.com, s2@mail.com]
   *  "Sender 1" <s1@mail.com>    =>      [s1@mail.com => "Sender 1"]
   *
   * Purposely, does NOT check if email address is valid (that should be done later)
   *
   * @param string $from
   *
   * @return array
   */
  protected function convertFromStringToArray($from) {
    $answer = [];
    $mails = [];
    if (strpos($from, ",")) {
      $mails = explode(",", $from);
    }
    else {
      $mails[] = trim($from);
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