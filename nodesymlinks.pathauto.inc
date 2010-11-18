<?php
// $Id$

/**
 * Function for creating aliases using Pathaouto API
 *
 * @param array $item
 * @param object $node
 * @param string $op
 */
function nodesymlinks_pathauto_create_alias($item, $node, $op = 'return') {
  // make sure that pathauto_create_alias is available
  module_load_include('inc', 'pathauto');
  $pathauto_alias = '';
  $source = 'node/' . $node->nid . '/mid/' . $item['mlid'];
  $entity_id = $node->nid;
  $language = $node->language;
  $type = $node->type;

  // generate placeholders/tokens for the current nodesymlink and node
  $data_object = $item;
  $data_object['node'] = $node;
  $node_placeholders = pathauto_get_placeholders('node', $node);
  $placeholders = pathauto_get_placeholders('nodesymlinks', $data_object);
  $placeholders['values'] += $node_placeholders['values'];
  nodesymlinks_reconstruct_tokens($placeholders);
  $data =& $placeholders;

  if (nodesymlinks_pathauto_version() == 1) {
    // For pathauto 6.x-1.x.
    $pathauto_alias = pathauto_create_alias('nodesymlinks', $op, $placeholders, $source, $entity_id, $type, $language);
  }
  else {
    // For 6.x-2.x (tested with 6.x-2.0-alpha3).
    $pathauto_alias = pathauto_create_alias('nodesymlinks', $op, $source, $data, $entity_id, $type, $language);
  }

  return $pathauto_alias;
}

/**
 * Helper function which detects Pathauto version. Uses static caching.
 *
 * @return int
 */
function nodesymlinks_pathauto_version() {
  static $version = NULL;

  if (is_null($version)) {
    $version = 0;
    // Get Pathauto version. @todo: find a way to successfully get version if downloaded via CVS.
    $pathauto = drupal_parse_info_file(drupal_get_path('module', 'pathauto') . '/pathauto.info');
    if ($pathauto['version']) {
      // Will return version formatted like: 6.x-2.x-dev.
      $parts = explode('-', $pathauto['version']);
      // Get '1' or '2'.
      $version = $parts[1][0];
    }
  }

  return $version;
}

/**
 * Generates tokens in right order - needed when merging tokens from different sources.
 *
 * @param array &$tokens
 */
function nodesymlinks_reconstruct_tokens(&$tokens) {
  unset($tokens['tokens']);
  $tokens['tokens'] = array_map('_nodesymlinks_key2token', array_keys($tokens['values']));
}

/**
 * Converts given string to token string "[string]".
 *
 * @param string $token
 *
 * @return string
 */
function _nodesymlinks_key2token($token) {
  return "[$token]";
}

/**
 * Generate aliases for all nodesymlinks without aliases.
 */
function _nodesymlinks_pathauto_bulkupdate() {
  $count = 0;
  $query = "SELECT m.mlid, m.link_path, alias.src, alias.dst
    FROM {menu_links} m LEFT JOIN {url_alias} alias ON m.link_path = alias.src
    WHERE m.module = 'nodesymlinks' AND alias.src IS NULL";
  $result = db_query_range($query, 0, variable_get('pathauto_max_bulk_update', 50));

  $placeholders = array();
  while ($nodesymlink_ref = db_fetch_object($result)) {
    $link_path = explode('/', $nodesymlink_ref->link_path);
    $nid = $link_path[1];
    $mlid = $nodesymlink_ref->mlid;
    $node = node_load($nid, NULL, TRUE);
    $item = menu_link_load($mlid);
    $node->src = $nodesymlink_ref->src;
    $node->dst = $nodesymlink_ref->dst;
    if (module_exists('taxonomy')) {
      // Must populate the terms for the node here for the category
      // placeholders to work
      $node->taxonomy = array_keys(taxonomy_node_get_terms($node));
    }
    if (nodesymlinks_pathauto_create_alias($item, $node, 'bulkupdate')) {
      $count ++;
    }
  }

  drupal_set_message(format_plural($count, 'Bulk generation of nodes completed, one alias generated.', 'Bulk generation of nodes completed, @count aliases generated.'));
}