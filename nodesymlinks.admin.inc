<?php
// $Id$


/**
 * Admin Settings Page callback. Just renders a listing of currently active
 * node alternate locations.
 */
function nodesymlinks_admin() {
  $output = '';
  $form = $items = $header = $rows = array();

  $mlids = db_query("SELECT mlid FROM {menu_links} WHERE module = 'nodesymlinks' AND link_path LIKE 'node/%%/mid/%%' ORDER BY mlid ASC");
  if ($mlids) {
    while ($row = db_fetch_array($mlids)) {
      $items[$row['mlid']] = menu_link_load($row['mlid']);
    }
  }

  // Render items
  if (count($items)) {
    $parent_options = menu_parent_options(menu_get_menus(), array(
      'mlid' => 0
    ));
    $output .= '<p>' . t('All alternate node locations') . '</p>';
    $header = array(
      t('Original Node'),
      t('Parent Item'),
      t('Link title'),
      t('Weight'),
      t('Links')
    );
    $rows = array();

    foreach ($items as $item) {
      $row = array();
      $parent_option_id = $item['menu_name'] . ':' . $item['plid'];
      $parent_name = isset($parent_options[$parent_option_id]) ? $parent_options[$parent_option_id] : '';
      $parent_menu_item = menu_link_load($item['plid']);
      $link_parts = explode('/', $item['link_path']);

      // Original node
      $row[] = l($item['options']['attributes']['title'], 'node/' . $link_parts[1]);
      // Parent
      $row[] = l($parent_name, $parent_menu_item['link_path']);
      // Alternate locations
      $row[] = l($item['link_title'], $item['link_path']);
      // Weight
      $row[] = $item['weight'];
      // Edit link
      $row[] = l(t('Edit'), 'node/' . $link_parts[1] . '/edit');

      $rows[] = $row;
    }
  }
  else {
    $output .= '<p>' . t('No node duplicates found.') . '</p>';
  }

  $output .= theme('table', $header, $rows);

  return $output;
}

/**
 * Admin settings form
 */
function nodesymlinks_settings($form_state) {
  $form = array();

  $form['breadcrumbs'] = array(
    '#type' => 'fieldset',
    '#title' => 'Breadcrumb settings'
  );
  $form['breadcrumbs']['nodesymlinks_crumbs_include_home'] = array(
    '#type' => 'checkbox',
    '#title' => t('Breadcrumbs starts with "Home" link'),
    '#default_value' => variable_get('nodesymlinks_crumbs_include_home', 1),
  );
  $form['breadcrumbs']['nodesymlinks_crumbs_lastcrumb'] = array(
    '#type' => 'radios',
    '#title' => t('Last breadcrumb is'),
    '#options' => array(
      'parent' => t('Parent of the current menu item'),
      'current_plain' => t('Current menu item title'),
      'current_link' => t('Current menu item as link')
    ),
    '#default_value' => variable_get('nodesymlinks_crumbs_lastcrumb', 'parent'),
  );

  $form['nodeform'] = array(
    '#type' => 'fieldset',
    '#title' => 'Node form'
  );
  $form['nodeform']['nodesymlinks_check_menuitem'] = array(
    '#type' => 'checkbox',
    '#title' => t('Node menu item check'),
    '#description' => t('Check if the node menu item is filled when some node symlink is defined. When enabled it will'.
            ' warn you every time when you try to add nodesymlink leaving <b>Menu link title</b> field empty.'),
    '#default_value' => variable_get('nodesymlinks_check_menuitem', 1),
  );

  $form['pathauto'] = array(
    '#type' => 'fieldset',
    '#title' => 'Pathauto'
  );
  $form['pathauto']['nodesymlinks_node_tokens'] = array(
    '#type' => 'checkbox',
    '#title' => t('Access node tokens'),
    '#default_value' => variable_get('nodesymlinks_node_tokens', 0),
  );

  return system_settings_form($form);
}