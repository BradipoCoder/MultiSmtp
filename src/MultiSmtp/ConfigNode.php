<?php
/**
 * Created by Adam Jakab.
 * Date: 20/06/16
 * Time: 11.53
 */

namespace MultiSmtp;


class ConfigNode {
  private static $configNodeTypeMachineName = 'multismtp_config';

  /**
   * Returns the name of the MultiSmtp configuration node's machine name if exists.
   * If id does not exixt, returns false.
   *
   * @return bool|string
   */
  public static function getConfigNodeTypeMachineName() {
    $answer = FALSE;
    $nodeTypes = node_type_get_names();
    if (array_key_exists(self::$configNodeTypeMachineName, $nodeTypes)) {
      $answer = self::$configNodeTypeMachineName;
    }
    return $answer;
  }

  /**
   * Returns an array of nodes of Config Nodes
   *
   * @return array
   */
  public static function getConfigNodeList() {
    $answer = [];
    $type = self::getConfigNodeTypeMachineName();
    if ($type) {
      $result = db_query("SELECT nid, title FROM node WHERE type = :type", [':type' => $type]);
      foreach ($result as $obj) {
        $answer[$obj->nid] = $obj->title;
      }
    }
    return $answer;
  }

  /**
   * Returns list of Emails extracted from Configurations with optional
   * param to add custom emails (for testing).
   *
   * @param array $optionals
   * @return array
   */
  public static function getListOfSendersWithConfiguration($optionals = []) {
    $answer = [];
    $listElements = self::getConfigNodeList();
    $nids = array_keys($listElements);
    $nodes = node_load_multiple($nids);
    foreach ($nodes as $node) {
      if ($senderEmail = self::getConfigNodeValue($node, 'field_smtp_email', false)) {
        $answer[$senderEmail] = $senderEmail . ' - ('.$node->title.')';
      }
    }
    if(is_array($optionals)) {
      $answer = array_merge($answer, $optionals);
    }
    return $answer;
  }

  /**
   * @param int $nid
   *
   * @return bool|mixed
   * @throws \Exception
   */
  protected static function getConfigNodeById($nid) {
    $configNode = node_load($nid);
    if (!$configNode) {
      throw new \Exception("MultiSmtp Configuration node with nid($nid) was not found!");
    }
    if ($configNode->type != self::$configNodeTypeMachineName) {
      throw new \Exception(
        "MultiSmtp Configuration node with nid($nid) is not of the expected("
        . self::$configNodeTypeMachineName . ") type!"
      );
    }
    return $configNode;
  }

  /**
   * @param string $sender
   *
   * @return \stdClass
   * @throws \Exception
   */
  public static function getConfigNodeByEmail($sender) {
    $nodes = node_load_multiple([], ["type" => self::$configNodeTypeMachineName]);
    if (!count($nodes)) {
      throw new \Exception("No MultiSmtp Configuration nodes were found!");
    }
    $found = FALSE;
    /** @var \stdClass $node */
    foreach ($nodes as $node) {
      if (self::getConfigNodeValue($node, 'field_smtp_email', '') == $sender) {
        $found = TRUE;
        break;
      }
    }
    if (!$found) {
      multismtp_debug('multismtp', 'Configuration for sender %sender not found. Using default config.', ['%sender' => $sender], WATCHDOG_DEBUG);
      $defaultConfigNid = variable_get('multismtp_default_config_node', FALSE);
      $node = self::getConfigNodeById($defaultConfigNid);
    }

    return $node;
  }

  /**
   * Handy method to get value from config node
   *
   * @param \stdClass $node
   * @param string $fieldName
   * @param mixed $default
   * @param string $valueKey
   *
   * @return bool
   */
  public static function getConfigNodeValue($node, $fieldName, $default = false, $valueKey = "value") {
    $answer = $default;
    if (isset($node->$fieldName)) {
      /** @var array $field */
      $field = $node->$fieldName;
      if(isset($field[LANGUAGE_NONE][0][$valueKey])) {
        $answer = $field[LANGUAGE_NONE][0][$valueKey];
      }
    }
    return $answer;
  }
}