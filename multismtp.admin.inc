<?php

/**
 * @file
 * Administrative page code for the smtp module.
 *
 */


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
    '#disabled' => true,
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
 * Submit handler().
 */
function multismtp_admin_settings_form_submit($form, &$form_state) {
  /*
  // Check if SMTP status has been changed.
  if (
    (!variable_get('smtp_on', FALSE) && $form_state['values']['smtp_on'])
    || (variable_get('smtp_on', FALSE) && !$form_state['values']['smtp_on'])
  ) {
    $mail_modes = variable_get('mail_system', array('default-system' => 'DefaultMailSystem'));

    // Turning on.
    if ($form_state['values']['smtp_on']) {
      variable_set('smtp_previous_mail_system', $mail_modes['default-system']);
      $mail_modes['default-system'] = 'SmtpMailSystem';
    }
    // Turning off.
    else {
      $mail_modes['default-system'] = variable_get('smtp_previous_mail_system', 'DefaultMailSystem');
    }

    variable_set('mail_system', $mail_modes);
  }
  */
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
  if(isset($form_state["input"]["test_from_email"]) && $form_state["input"]["test_from_email"]) {
    $emailToTest = $form_state["input"]["test_from_email"];
  }

  if($emailToTest) {
    $sender = '';
    if($emailToTest == 'custom') {
      if(isset($form_state["input"]["test_from_email_custom"]) && $form_state["input"]["test_from_email_custom"]) {
        $sender = $form_state["input"]["test_from_email_custom"];
      }
    } else {
      $sender = $emailToTest;
    }

    $recepient = '';
    if(isset($form_state["input"]["test_to_email"]) && $form_state["input"]["test_to_email"]) {
      $recepient = $form_state["input"]["test_to_email"];
    }

    if($sender && $recepient) {
      _multismtp_admin_tests_test_configuration($sender, $recepient);
    }
  }



  $senders = \MultiSmtp\ConfigNode::getListOfSendersWithConfiguration(['custom'=>'Custom Email']);

  $form['test']['test_from_email'] = [
    '#type' => 'select',
    '#title' => t('From'),
    '#options' => $senders,
    '#default_value' => $emailToTest,
    '#required' => true,
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
    '#required' => true,
    '#description' => t('Send mail to.'),
  ];

  $form['test']['test'] = [
    '#type' => 'button',
    '#value' => t('Test'),
    '#executes_submit_callback' => false,
    '#submit' => array('multismtp_admin_tests_test'),
  ];

  return $form;
}

/**
 * Tests a configuration node
 *
 * @param string $emailToTest
 * @param string $recepient
 *
 * @throws \Exception
 */
function _multismtp_admin_tests_test_configuration($emailToTest, $recepient) {
  try {
    $message = drupal_mail('multismtp', 'test', $recepient, LANGUAGE_NONE, [], $emailToTest, true);
    if(isset($message['result']) && $message['result']) {
      drupal_set_message(t("E-mail sent to $recepient."));
    } else {
      drupal_set_message("Send failed. " . json_encode($message), 'warning');
    }
  } catch(\Exception $e) {
    drupal_set_message("Send failed with Exception: " . $e->getMessage(), 'error');
  }
}