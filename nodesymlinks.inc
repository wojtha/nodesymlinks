<?php
// $Id$


/**
 * Delete menulink and alias of given item. It is not needed to have item fully
 * loaded. Required kes are 'link_path' and 'mlid'.
 *
 * @param item $item
 */
function nodesymlinks_item_delete(&$item) {
  if (module_exists('path')) {
    path_set_alias($item['link_path']);
  }
  menu_link_delete($item['mlid']);
  // Delete our stored copy
  db_query("DELETE FROM {nodesymlinks_link_storage} WHERE mlid = %d", $item['mlid']);
}

/**
 * Load all duplicate menulinks from database for given node nid. Returns array
 * of mlid and link_path.
 *
 * @param int $nid
 * @return array
 */
function nodesymlinks_get_items_by_nid($nid) {
  $items = array();
  $result = db_query("SELECT mlid,link_path FROM {menu_links} WHERE link_path LIKE 'node/%d/mid/%%' AND module = 'nodesymlinks' ", $nid);
  while ($item = db_fetch_array($result)) {
    $items[$item['mlid']] = $item;
  }
  return $items;
}

/**
 * Implementation of hook_nodeapi() OP: Insert & Update.
 *
 * @see nodesymlinks_nodeapi()
 */
function _nodesymlinks_nodeapi_insert_update(&$node, $op) {
  $count_deleted = $count_saved = 0;
  $items = (array) $node->menu['nodesymlinks']['items'];
  $update_items = array();
  $db_items = nodesymlinks_get_items_by_nid($node->nid);

  // Save all menulinks from form (insert/update is handled by Menu API)
  foreach ($items as &$item) {
    // load defaults and process item before save
    nodesymlinks_item_process($item, $node);
    if (! $item['is_new']) {
      $update_items[] = $item['mlid'];
    }
    if (nodesymlinks_item_save($item, $node)) {
      ++ $count_saved;
    }
    else {
      drupal_set_message(t('There was an error when saving the menu link.'), 'error');
    }
  }

  // Detect which items exists in database, but it was not saved.
  // Delete them.
  $items_to_delete = array_diff(array_keys($db_items), $update_items);
  while ($mlid = array_pop($items_to_delete)) {
    nodesymlinks_item_delete($db_items[$mlid]);
    ++$count_deleted;
  }

  drupal_set_message(
      format_plural($count_saved, 'Saved 1 nodesymlink.', 'Saved @count nodesymlinks.') . ' ' .
      format_plural($count_deleted, 'Deleted 1 nodesymlink.', 'Deleted @count nodesymlinks.')
  );
}

/**
 * Implementation of hook_nodeapi() OP: Delete.
 *
 * @see nodesymlinks_nodeapi()
 */
function _nodesymlinks_nodeapi_delete(&$node, $op) {
  // Delete all menu module links that point to this node.
  $result = db_query("SELECT mlid FROM {menu_links} WHERE link_path LIKE 'node/%d/mid/%%' AND module = 'nodesymlinks' ", $node->nid);
  while ($m = db_fetch_array($result)) {
    menu_link_delete($m['mlid']);
  }
  // Care about our aliases
  db_query("DELETE FROM {url_alias} WHERE src LIKE 'node/%d/mid/%%'", $node->nid);
  // Delete our nodesymlinks storage
  db_query('DELETE FROM {nodesymlinks_link_storage} WHERE nid=%d', $node->nid);
  break;
}

/**
 * Implementation of hook_nodeapi() OP: Prepare & Presave.
 *
 * @see nodesymlinks_nodeapi()
 */
function _nodesymlinks_nodeapi_prepare_presave(&$node, $op) {
  // Prepare the node for the edit form so that $node->menu always exists.
  $items = array();
  if (isset($node->nid)) {
    // Check all menus if a link does not exist in the default menu.
    $mlids = db_query("SELECT mlid FROM {menu_links} WHERE module = 'nodesymlinks' AND link_path LIKE 'node/%d/mid/%%' ORDER BY mlid ASC", $node->nid);
    if ($mlids) {
      while ($row = db_fetch_array($mlids)) {
        $items[$row['mlid']] = menu_link_load($row['mlid']);
      }
    }
  }
  // Set default values.
  $node->nodesymlinks['items'] = $items;

  //array('link_title' => '', 'mlid' => 0, 'plid' => 0, 'menu_name' => $menu_name, 'weight' => 0, 'options' => array(), 'module' => 'menu', 'expanded' => 0, 'hidden' => 0, 'has_children' => 0, 'customized' => 0);
}

/**
 * Helper function to generate custom nodesymlinks form item.
 *
 * @param item $item
 * @return form_elements
 */
function _nodesymlinks_form_field($item = NULL) {
  // field_number is used to handle deletions.
  static $field_number = 0;

  if (is_null($item) || isset($item['new_item'])) {
    // load default properties
    $item = array(
      'link_title' => isset($item['link_title']) ? $item['link_title'] : '',
      'mlid' => 0,
      'plid' => 0,
      'menu_name' => 'navigation',
      'weight' => 0,
      'options' => array(),
      'module' => 'nodesymlinks',
      'expanded' => 0,
      'hidden' => 0,
      'has_children' => 0,
      'customized' => 0
    );
  }

  $item_form = array();
  $item_form['#tree'] = TRUE;

  // TODO simplify this - whole array can be stored in Value type
  foreach (array(
    'mlid',
    'module',
    'hidden',
    'has_children',
    'customized',
    'options',
    'expanded',
    'parent_depth_limit'
  ) as $key) {
    $item_form[$key] = array(
      '#type' => 'value',
      '#value' => $item[$key]
    );
  }

  // Generate a list of possible parents (not including this item or descendants).
  $options = menu_parent_options(menu_get_menus(), $item);
  // Generate all tree $item['mlid'] == 0
  //$options = menu_parent_options(menu_get_menus(), array('mlid' => 0));
  $default = ! empty($item['parents']) ? $item['parents'] : $item['menu_name'] . ':' . $item['plid'];
  if (! isset($options[$default])) {
    $default = array(
      'primary-links:0'
    );
  }

  $item_form['parents'] = array(
    '#type' => 'select',
    //'#title' => t('Parent items'),
    '#default_value' => $default,
    '#options' => $options,
    '#multiple' => FALSE,
    '#description' => '',
    '#attributes' => array(
      'class' => 'menu-title-select'
    )
  );

  $item_form['link_title'] = array(
    '#type' => 'textfield',
    //'#title' => t('Menu link title'),
    '#default_value' => $item['link_title'],  // check_plain() is unnecessary here
    '#description' => '',  // and also it translates titles to HTML entities
    '#required' => FALSE
  );

  $item_form['weight'] = array(
    '#type' => 'weight',
    //'#title' => t('Weight'),
    '#delta' => 50,
    '#default_value' => $item['weight'],
    '#description' => ''
  );

  $item_form['delete'] = array(
    '#type' => 'checkbox',
    '#title' => ''
  );

  return $item_form;
}

/**
 * Implementation of hook_form_alter(). Adds nodesymlinks item fields to the node form.
 */
function _nodesymlinks_form_alter(&$form, $form_state, $form_id) {
  // Note - doing this to make sure the delete checkbox stays in the form.
  $form['#cache'] = TRUE;

  if (isset($form_state['values'])) {
    $nodesymlinks = (array) $form_state['values']['menu']['nodesymlinks']['items'];
  }
  else {
    $nodesymlinks = (array) $form['#node']->nodesymlinks['items'];
  }

  $collapsed = count($nodesymlinks) ? TRUE : FALSE;

  $form['menu']['nodesymlinks'] = array(
    '#type' => 'fieldset',
    '#title' => t('Show in More Locations'),
    '#access' => user_access('administer menu') || user_access('administer nodesymlinks'),
    '#collapsible' => TRUE,
    '#collapsed' => $collapsed,
    '#tree' => TRUE,
    '#weight' => 1,
    '#prefix' => '',
    '#suffix' => '',
    '#attributes' => array(
      'class' => 'nodesymlinks-items-form'
    )
  );

  // Submenu Tree Settings & Siblingmenu Tree Settings
  // We want to display Symlinks after Menu Links and before SubmenuTree Settings
  if (isset($form['menu']['submenutree'])) {
    $form['menu']['submenutree']['#weight'] = 2;
    $form['menu']['siblingmenutree']['#weight'] = 2;
  }

  $delta = 0;
  $form['menu']['nodesymlinks']['items']['#tree'] = TRUE;
  $form['menu']['nodesymlinks']['items']['#theme'] = 'nodesymlinks_form_items';

  foreach ($nodesymlinks as $item) {
    $form['menu']['nodesymlinks']['items'][$delta ++] = _nodesymlinks_form_field($item);
  }
  if (isset($form_state['item_count']['nodesymlinks'])) {
    $new_max_delta = $form_state['item_count']['nodesymlinks'] - 1;
    for ($delta; $delta <= $new_max_delta; ++ $delta) {
      $form['menu']['nodesymlinks']['items'][$delta] = _nodesymlinks_form_field(array(
        'new_item' => TRUE,
        'link_title' => $form['#node']->title
      ));
    }
  }

  $form['menu']['nodesymlinks']['#description'] = t('This module allows you to show this node\'s content within multiple places in your site.<br />');
  $form['menu']['nodesymlinks']['#description'] .= '<b>' . t('Parent item') . ':</b> ' . t('The place where this page\'s content will appear. It will use the selected parent\'s breadcrumbs and sidebar. Items in the hierarchy deeper than ' . (MENU_MAX_DEPTH - 1) . ' levels may not appear.', array(
    '!maxdepth' => MENU_MAX_DEPTH
  ));
  $form['menu']['nodesymlinks']['#description'] .= ' <br /><b>' . t('Menu link title') . ':</b> ' . t('The link text corresponding to this item that should appear in the menu.');
  $form['menu']['nodesymlinks']['#description'] .= ' <br /><b>' . t('Weight') . ':</b> ' . t('Optional. In the menu, a higher weight number will sink down and a lower weight will be positioned closer to the top.');
  $form['menu']['nodesymlinks']['#description'] .= ' <br /><b>' . t('Delete') . ':</b> ' . t('To delete nodesymlinks, first check the "Delete" checkbox in the desired rows, then click "Delete Location(s)".');
  $form['menu']['nodesymlinks']['#description'] .= ' <br /><b>' . t('Saving') . ': ' . t('When you are done editing, you must click "Save" at the very bottom of the page to save your changes.') . '</b>';

  // We add a div around the new content to tell AHAH to let this fade in.
  //$form['nodesymlinks']['items']['#prefix'] = '<div id="nodesymlinks-items-ahah">';
  //$form['nodesymlinks']['items']['#suffix'] = '</div>';
  //NOTE: Markup form element has no prefix and suffix, so the lines above will be ignored by FAPI anyway.
  //      AHAH markers are currently injected by items theme function


  $form['menu']['nodesymlinks']['delete'] = array(
    '#type' => 'submit',
    '#value' => t('Delete Location(s)'),
    '#submit' => array(
      'nodesymlinks_node_form_submit_delete'
    ),
    '#ahah' => array(
      'path' => 'nodesymlinks/ahah',
      'wrapper' => 'nodesymlinks-items-ahah',
      'method' => 'replace',
      'effect' => 'fade'
    )
  );

  $form['menu']['nodesymlinks']['add'] = array(
    '#type' => 'submit',
    '#value' => t('Add Location'),
    '#submit' => array(
      'nodesymlinks_node_form_submit_add'
    ),
    '#ahah' => array(
      'path' => 'nodesymlinks/ahah',
      'wrapper' => 'nodesymlinks-items-ahah',
      'method' => 'replace',
      'effect' => 'fade'
    )
  );
  $form['#validate'][] = 'nodesymlinks_node_form_validate';

  if (isset($form_state['clicked_button'])) {
    $clicked_val = $form_state['clicked_button']['#value'];
    if ($clicked_val == t('Add Location') || $clicked_val == t('Delete Location(s)')) {
      $form['menu']['#collapsed'] = FALSE;
      $form['menu']['nodesymlinks']['#collapsed'] = FALSE;
    }
  }
}

/**
 * Add new symlinks item, submit handler for symlinks at node form
 *
 * @see nodesymlinks_form_alter()
 */
function nodesymlinks_node_form_submit_add($form, &$form_state) {
  // Set the form to rebuild and run submit handlers.
  node_form_submit_build_node($form, $form_state);

  // Make the changes we want to the form state.
  if ($form_state['clicked_button']['#value'] == $form['menu']['nodesymlinks']['add']['#value']) {
    $items = element_children($form['menu']['nodesymlinks']['items']);
    // Define new item count - it will be used in form process to add new items
    $form_state['item_count']['nodesymlinks'] = count($items) + 1;
  }
}

/**
 * Delete selected symlinks items, submit handler for symlinks at node form
 *
 * @see nodesymlinks_form_alter()
 */
function nodesymlinks_node_form_submit_delete($form, &$form_state) {
  if ($form_state['clicked_button']['#value'] == $form['menu']['nodesymlinks']['delete']['#value']) {
    $item_keys = element_children($form['menu']['nodesymlinks']['items']);
    $values_items = & $form_state['values']['menu']['nodesymlinks']['items'];
    $form_items = & $form['menu']['nodesymlinks']['items'];
    foreach ($item_keys as $delta) {
      // remove item from both values and form array
      // however this doesn't remove item from rendered form items
      if (! empty($values_items[$delta]['delete']) && $values_items[$delta]['delete']) {
        unset($values_items[$delta]);
        unset($form_items[$delta]);
      }
    }
  }

  // Set the form to rebuild and run submit handlers.
  node_form_submit_build_node($form, $form_state);
}

/**
 * Menu callback for AHAH additions. Most of the code taken from
 * core poll module.
 *
 * @see poll_choice_js()
 */
function nodesymlinks_ahah_handler() {
  module_load_include('inc', 'node', 'node.pages');

  $form_state = array('storage' => NULL, 'submitted' => FALSE);
  $form_build_id = $_POST['form_build_id'];
  // Get the form from the cache.
  $form = form_get_cache($form_build_id, $form_state);
  $args = $form['#parameters'];
  $form_id = array_shift($args);
  // We will run some of the submit handlers so we need to disable redirecting.
  $form['#redirect'] = FALSE;
  // We need to process the form, prepare for that by setting a few internals
  // variables.
  $form['#post'] = $_POST;
  $form['#programmed'] = FALSE;
  $form_state['post'] = $_POST;
  // Build, validate and if possible, submit the form.
  drupal_process_form($form_id, $form, $form_state);
  // This call recreates the form relying solely on the form_state that the
  // drupal_process_form set up.
  $form = drupal_rebuild_form($form_id, $form_state, $args, $form_build_id);
  // Render the new output.
  $subform = $form['menu']['nodesymlinks']['items'];
  $output = theme('status_messages') . drupal_render($subform);

  $GLOBALS['devel_shutdown'] = FALSE;
  drupal_json(array('status' => TRUE, 'data' => $output));
}

/**
 * Create and returns item path
 *
 * @param item $item
 * @param node $node
 * @return string
 */
function nodesymlinks_create_item_path(&$item, &$node) {
  return 'node/' . $node->nid . '/mid/' . $item['mlid'];
}

/**
 * Load item defaults and process item before is saved
 *
 * @param item $item
 * @param node $node
 */
function nodesymlinks_item_process(&$item, &$node) {
  $item_defaults = array(
    'link_title' => '',
    'mlid' => 0,
    'plid' => 0,
    'menu_name' => 'navigation',
    'weight' => 0,
    'options' => array(),
    'module' => 'nodesymlinks',
    'expanded' => 0,
    'hidden' => 0,
    'has_children' => 0,
    'customized' => 0
  );
  // load defaults
  $item += $item_defaults;

  // mark new item, $item[mlid]=0 is changed during menu_link_save, so
  // it can't be used to marking
  $item['is_new'] = ($item['mlid'] == 0) ? TRUE : FALSE;
  if (! empty($item['parents'])) {
    list($item['menu_name'], $item['plid']) = explode(':', $item['parents']);
  }
  if (! $item['customized']) {
    $item['options']['attributes']['title'] = trim($node->title);
  }

  // @TODO: maybe we can remove this line, if we validate not empty title field and fill the field with the default value
  $item['link_title'] = trim($item['link_title']) ? trim($item['link_title']) : $node->title;

  $item['link_path'] = nodesymlinks_create_item_path($item, $node);
}

/**
 * Save item to database, create alias for duplicate menulink if original node
 * has one. Returns TRUE if saving was successfull, else returns FALSE.
 *
 * @param item $item
 * @param node $node
 * @return bool
 */
function nodesymlinks_item_save(&$item, &$node) {
  /**
   * If this function is updated, please update its counterpart in
   * nodesymlinks.install, which exists to restore deleted menu_links during
   * module re-enable.
   */
  if (menu_link_save($item)) {
    // if the item is new, we need to save it second time - now with real mlid
    if ($item['is_new']) {
      $item['link_path'] = 'node/' . $node->nid . '/mid/' . $item['mlid'];
      menu_link_save($item);
    }

    // Because menu_links is wiped out on module disable (not Uninstall),
    // we need to store this for later re-use if we detect a module re-enable.
    if ($item['is_new']) {
      // Save data in our permanent store.
      db_query("INSERT INTO {nodesymlinks_link_storage} (
        mlid, nid, item_data) VALUES (
          %d, %d, '%s')", $item['mlid'], $node->nid, serialize($item));
    }
    else {
      // Update in permanent store
      db_query("UPDATE {nodesymlinks_link_storage} SET
        nid = %d, item_data = '%s' WHERE
        mlid = %d", $node->nid, serialize($item), $item['mlid']);
    }

    if (module_exists('path')) {
      // Integrate with pathauto.
      if (module_exists('pathauto')) {
        $op = ($item['is_new']) ? 'insert' : 'update';
        _nodesymlinks_include('pathauto');
        nodesymlinks_pathauto_create_alias($item, $node, $op);
      }
      else {
        // make alias
        $node_alias = ($node->path == 'node/' . $node->nid) ? NULL : $node->path;
        if ($node_alias) {
          path_set_alias($item['link_path'], $node_alias . '_' . $item['mlid']);
        }
      }
    }

    return TRUE;
  }

  return FALSE;
}

/**
 * Theme nodesymlinks form items
 *
 * @param element $form
 * @return HTML
 */
function theme_nodesymlinks_form_items($form) {
  $output = '';
  $output .= '<div id="nodesymlinks-items-ahah">';
  $items = element_children($form);
  $rows = array();
  foreach ($items as $delta) {
    $row = array();
    $row[] = $delta;
    $row[] = drupal_render($form[$delta]['delete']);
    $row[] = drupal_render($form[$delta]['parents']);
    $row[] = drupal_render($form[$delta]['link_title']);
    $row[] = drupal_render($form[$delta]['weight']);
    $rows[] = $row;
  }
  $header = array('#', t('Delete'), t('Parent item'), t('Link title'), t('Weight'));
  $output .= theme_table($header, $rows);
  $output .= drupal_render($form);
  $output .= '</div>';
  return $output;
}