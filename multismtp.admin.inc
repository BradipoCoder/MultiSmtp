<?php
/**
 * Administrative settings.
 *
 * @return array
 *   An array containing form items to place on the module settings page.
 */
function multismtp_admin_settings() {
  if (variable_get('multismtp_enabled', 0)) {
    drupal_set_message(t('MultiSmtp module is ACTIVE.'));
  }
  else {
    drupal_set_message(t('MultiSmtp module is INACTIVE.'));
  }

  $logging = variable_get('multismtp_debugging', MULTISMTP_LOGGING_ERRORS);

  $form['onoff'] = array(
    '#type' => 'fieldset',
    '#title' => t('Install options'),
  );

  $form['onoff']['multismtp_enabled'] = array(
    '#type' => 'radios',
    '#title' => t('Turn this module on or off'),
    '#default_value' => variable_get('multismtp_enabled', FALSE),
    '#options' => array(1 => t('On'), 0 => t('Off')),
    '#description' => t('To uninstall this module you must turn it off here first.'),
  );

  $form['onoff']['multismtp_deliver'] = array(
    '#type' => 'radios',
    '#title' => t('Turn on delivery of emails'),
    '#default_value' => variable_get('multismtp_deliver', TRUE),
    '#options' => array(1 => t('On'), 0 => t('Off')),
    '#description' => t('With this option turned off, email messages will be queued up and processed as normal, but not actually delivered. This option should only be used for testing purposes.'),
  );

  $form['onoff']['multismtp_queue'] = array(
    '#type' => 'checkbox',
    '#title' => t('Send mail by queue'),
    '#default_value' => variable_get('multismtp_queue', FALSE),
    '#description' => t('Mails will be sent by drupal queue api.'),
  );

  $form['onoff']['multismtp_queue_fail'] = array(
    '#type' => 'checkbox',
    '#title' => t('Retry sending mail on error.'),
    '#default_value' => variable_get('multismtp_queue_fail', FALSE),
    '#description' => t('Mails will be added to the queue and sent by drupal queue api.'),
  );

  $form['config'] = array(
    '#type' => 'fieldset',
    '#title' => t('SMTP configuration settings'),
  );
  $form['config']['multismtp_node_type'] = array(
    '#type' => 'textfield',
    '#title' => t('Configuration Node Type'),
    '#default_value' => \MultiSmtp\ConfigNode::getConfigNodeTypeMachineName(),
    '#disabled' => TRUE,
    '#description' => t('The machine name of the node type where you are storing your smtp configurations.'),
  );
  $form['config']['multismtp_default_config_node'] = array(
    '#type' => 'select',
    '#title' => t('Default Configuration Node'),
    '#options' => \MultiSmtp\ConfigNode::getConfigNodeList(),
    '#default_value' => variable_get('multismtp_default_config_node', ''),
    '#description' => t('The default configuration node to use when no matching configuration is found.'),
  );


  $form['debugging'] = array(
    '#type' => 'fieldset',
    '#title' => t('Debugging and logging'),
  );

  $logging_options = array(
    MULTISMTP_LOGGING_ALL => t('Log everything'),
    MULTISMTP_LOGGING_ERRORS => t('Errors only'),
    MULTISMTP_LOGGING_NONE => t('No logging'),
  );

  $form['debugging']['multismtp_debugging'] = array(
    '#type' => 'select',
    '#title' => t('Logging'),
    '#options' => $logging_options,
    '#default_value' => $logging,
    '#description' => t('Choose the appropriate log level. "Log everything" will log errors and informational messages when an email is sent. "Errors only" will only create a log entry when sending failed. "No logging" will disable all logging for this module.'),
  );


  $form['#submit'][] = 'multismtp_admin_settings_form_submit';

  return system_settings_form($form);
}


/**
 * Validation for the administrative settings form.
 *
 * @param form
 *   An associative array containing the structure of the form.
 * @param form_state
 *   A keyed array containing the current state of the form.
 */
function multismtp_admin_settings_validate($form, &$form_state) {
  if (empty($form_state['values']['multismtp_node_type'])) {
    form_set_error('multismtp_node_type', t('Missing configuration node type!'));
  }
}

/**
 * Submit handler.
 *
 * @param array $form
 * @param array $form_state
 */
function multismtp_admin_settings_form_submit($form, &$form_state) {
  if (
    (!variable_get('multismtp_enabled', FALSE)
     && $form_state['values']['multismtp_enabled'])
    || (variable_get('multismtp_enabled', FALSE)
        && !$form_state['values']['multismtp_enabled'])
  ) {
    $mail_systems = variable_get('mail_system', ['default-system' => 'DefaultMailSystem']);

    // Turning on.
    if ($form_state['values']['multismtp_enabled']) {
      variable_set('multismtp_previous_mail_systems', $mail_systems);
      foreach ($mail_systems as &$mail_system) {
        $mail_system = 'MultiSmtpMailSystem';
      }
    }
    // Turning off.
    else {
      $mail_systems = variable_get('multismtp_previous_mail_systems', ['default-system' => 'DefaultMailSystem']);
    }

    variable_set('mail_system', $mail_systems);
  }
}


/**
 * Administrative tests.
 *
 * @return array
 *   The form
 */
function multismtp_admin_tests($form, &$form_state) {
  /** @var \stdClass $user */
  global $user;

  $emailToTest = '';
  if (isset($form_state["input"]["test_from_email"])
      && $form_state["input"]["test_from_email"]
  ) {
    $emailToTest = $form_state["input"]["test_from_email"];
  }

  if ($emailToTest) {
    $sender = '';
    if ($emailToTest == 'custom') {
      if (isset($form_state["input"]["test_from_email_custom"])
          && $form_state["input"]["test_from_email_custom"]
      ) {
        $sender = $form_state["input"]["test_from_email_custom"];
      }
    }
    else {
      $sender = $emailToTest;
    }

    $recipient = '';
    if (isset($form_state["input"]["test_to_email"])
        && $form_state["input"]["test_to_email"]
    ) {
      $recipient = $form_state["input"]["test_to_email"];
    }

    if ($sender && $recipient) {
      $params = [
        'subject' => isset($form_state["input"]["subject"]) ? $form_state["input"]["subject"] : "",
        'body' => isset($form_state["input"]["body"]) ? $form_state["input"]["body"] : "",
        'html' => isset($form_state["input"]["html"])
                  && $form_state["input"]["html"],
      ];
      _multismtp_admin_tests_test_configuration($sender, $recipient, $params);
    }
  }

  $senders = \MultiSmtp\ConfigNode::getListOfSendersWithConfiguration(['custom' => 'Custom Email']);

  $form['test']['test_from_email'] = [
    '#type' => 'select',
    '#title' => t('From'),
    '#options' => $senders,
    '#default_value' => $emailToTest,
    '#required' => TRUE,
    '#description' => t('Select a sender.'),
  ];

  $form['test']['test_from_email_custom'] = [
    '#type' => 'textfield',
    '#title' => t('Custom From'),
    '#description' => t('Custom sender.'),
    '#states' => [
      'enabled' => [
        ':input[name="test_from_email"]' => array('value' => 'custom'),
      ],
      'required' => [
        ':input[name="test_from_email"]' => array('value' => 'custom'),
      ],
    ],
  ];

  $form['test']['test_from_name'] = [
    '#type' => 'textfield',
    '#title' => t('From name'),
    '#description' => t('The name of the sender.'),
  ];

  $form['test']['test_to_email'] = [
    '#type' => 'textfield',
    '#title' => t('To'),
    '#default_value' => $user->mail,
    '#required' => TRUE,
    '#description' => t('Send mail to.'),
  ];

  $form['test']['subject'] = [
    '#type' => 'textfield',
    '#title' => t('Subject'),
    '#default_value' => "MultiSmtp Test E-mail",
    '#description' => t('The subject of the message.'),
  ];

  $form['test']['body'] = [
    '#type' => 'textarea',
    '#title' => t('Body'),
    '#default_value' => "This E-mail was sent through the <strong>multismtp</strong> module.\n\nHave a good day!",
    '#description' => t('The body of the message. You can use Html markup as well.'),
  ];

  $form['test']['html'] = array(
    '#type' => 'checkbox',
    '#title' => t('Html'),
    '#description' => t('Senf Html or Text message.'),
  );

  $form['test']['test'] = [
    '#type' => 'button',
    '#value' => t('Test'),
    '#executes_submit_callback' => FALSE,
    '#submit' => array('multismtp_admin_tests_test'),
  ];

  return $form;
}

/**
 * Tests a configuration node by sending a mail through drupal_mail
 *
 * @param string $emailToTest
 * @param string $recipient
 * @param array $params
 *
 * @throws \Exception
 */
function _multismtp_admin_tests_test_configuration($emailToTest, $recipient,
                                                   $params = []) {
  try {
    $message = drupal_mail('multismtp', 'test', $recipient, LANGUAGE_NONE, $params, $emailToTest, TRUE);
    if (isset($message['result']) && $message['result']) {
      drupal_set_message(t("E-mail sent to $recipient."));
    }
    else {
      drupal_set_message("Send failed. " . json_encode($message), 'warning');
    }
  } catch(\Exception $e) {
    drupal_set_message("Send failed with Exception: " . $e->getMessage(), 'error');
  }
}