<?php

/**
 * @file
 * Functions for forms.
 */


/********* thePlatform signIn / Account ***********************/

/**
 * Page callback to return signin / account forms.
 */
function media_theplatform_mpx_page_account() {
  $form_signin = drupal_get_form('media_theplatform_mpx_form_signin_theplatform');
  $output = render($form_signin);
  $cron_form = '';

  // If we have signin, display account list.
  if (media_theplatform_mpx_check_token()) {
    $account_list = media_theplatform_mpx_get_accounts_select();
    $import_account = media_theplatform_mpx_variable_get('import_account');
    // If current account doesn't exist, error msg.
    if ($import_account && !array_key_exists($import_account, $account_list)) {
      $output .= '<div class="message">' . t('The current Import Account (:name) was not found in thePlatform; it may have been disabled or deleted.  Please select a new Account to use as the Import Account.', array(':name' => rawurldecode($import_account))) . '</div>';
    }
    elseif ($import_account) {
      $settings_form = drupal_get_form('media_theplatform_mpx_form_settings');
    }
    $form_account = drupal_get_form('media_theplatform_mpx_form_account_theplatform', $account_list);
    $output .= render($form_account);
    $output .= render($settings_form);
  }
  return $output;
}

/**
 * Form constructor for thePlatform username/password.
 *
 * @ingroup forms
 */
function media_theplatform_mpx_form_signin_theplatform($form, &$form_state) {
  $collapsed = FALSE;
  $action_label = t('Connect to MPX');
  // If token exists already, collapse this form.
  if (media_theplatform_mpx_variable_get('token')) {
    $collapsed = TRUE;
    $action_label = t('Update');
  }
  $form['account'] = array(
    '#type' => 'fieldset',
    '#title' => t('mpx Login'),
    '#description' => t('Enter your administrator login for thePlatform.com.'),
    '#collapsible' => $collapsed,
    '#collapsed' => $collapsed,
  );
  $form['account']['theplatform_username'] = array(
    '#type' => 'textfield',
    '#title' => t('Username'),
    '#description' => t('Prefix your username with "mpx/". e.g. "mpx/admin@example.com".'),
    '#maxlength' => 255,
    '#required' => TRUE,
    '#default_value' => media_theplatform_mpx_variable_get('username'),
  );
  $form['account']['theplatform_password'] = array(
    '#type' => 'password',
    '#title' => t('Password'),
    '#maxlength' => 255,
    '#required' => TRUE,
  );
  $form['account']['actions'] = array('#type' => 'actions');
  $form['account']['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => $action_label,
  );
  return $form;
}

/**
 * Form submit handler for media_theplatform_mpx_form_signin_theplatform().
 */
function media_theplatform_mpx_form_signin_theplatform_submit($form, &$form_state) {
  media_theplatform_mpx_variable_set('username', $form_state['values']['theplatform_username']);
  media_theplatform_mpx_variable_set('password', $form_state['values']['theplatform_password']);
  if (media_theplatform_mpx_signin()) {
    drupal_set_message(t('Login successful.  Please select your Import Account.'));
  }
  else {
    form_set_error('media_theplatform_mpx', t('Invalid login.'));
  }
}

/**
 * Form constructor for selecting Import Account.
 *
 * @ingroup forms
 */
function media_theplatform_mpx_form_account_theplatform($form, &$form_state, $account_list) {
  $form['accounts'] = array(
    '#type' => 'fieldset',
    '#title' => t('Import Account'),
    '#collapsible' => FALSE,
    '#collapsed' => FALSE,
  );
  $form['accounts'][media_theplatform_mpx_NAMESPACE . 'import_account'] = array(
    '#type' => 'select',
    '#title' => t('Select Account:'),
    '#options' => $account_list,
    '#required' => TRUE,
    '#empty_option' => t('- Select -'),
    '#default_value' => media_theplatform_mpx_variable_get('import_account'),
    '#description' => t('Set the account from which to import mpxPlayers and Feeds from.'),
  );
  $form['accounts']['actions'] = array('#type' => 'actions');
  $form['accounts']['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Set Import Account'),
  );
  return $form;
}

/**
 * Form submit handler for media_theplatform_mpx_form_account_theplatform().
 */
function media_theplatform_mpx_form_account_theplatform_submit($form, &$form_state) {
  $import_account = $form_state['values'][media_theplatform_mpx_NAMESPACE . 'import_account'];
  // Write mpx_log record.
  global $user;
  $log = array(
    'uid' => $user->uid,
    'type' => 'settings',
    'type_id' => NULL,
    'action' => 'import account',
    'details' => 'new value = ' . $import_account . ' | old value = ' . media_theplatform_mpx_variable_get('import_account'),
  );
  media_theplatform_mpx_insert_log($log);
  media_theplatform_mpx_variable_set('import_account', $import_account);

  // Query MPX to get the account$pid for the selected account.
  $url = 'http://access.auth.theplatform.com/data/Account?schema=1.3.0&form=json&byDisabled=false&byTitle=' . $import_account . '&token=' . media_theplatform_mpx_variable_get('token');
  $result = drupal_http_request($url);
  $result_data = drupal_json_decode($result->data);

  // Set account_pid variable.
  media_theplatform_mpx_variable_set('account_pid', $result_data['entries'][0]['placcount$pid']);
  // Set account_id variable.
  media_theplatform_mpx_variable_set('account_id', $result_data['entries'][0]['id']);

  // Import all players for this account.
  $import = media_theplatform_mpx_import_all_players('manual');
  drupal_set_message(t('Import account set.') . ' ' . l(t('All mpxPlayers imported from the Import account.'), MPX_PATH_PLAYER));
  
  // Reset last notification, since it's account specific.
  media_theplatform_mpx_variable_set('last_notification', FALSE);
}

/**
 * Returns TRUE if token and import_account set.  If not, drupal_set_message and returns FALSE.
 */
function media_theplatform_mpx_check_account_settings() {
  if (!media_theplatform_mpx_check_token() || !media_theplatform_mpx_variable_get('import_account')) {
    if (user_access('administer mpx account')) {
      drupal_set_message(t('Your mpx Account is not configured.') . ' ' . l(t('Configure mpx Account.'), MPX_PATH_ADMIN), 'error');
    }
    else {
      drupal_set_message(t('Your mpx Account is not configured.') . ' ' . t('Please contact your System Administrator.'), 'error');
    }
    return FALSE;
  }
  return TRUE;
}

/**
 * System settings form constructor for Cron settings.
 *
 * @ingroup forms
 */
function media_theplatform_mpx_form_settings() {
  $form['sync'] = array(
    '#type' => 'fieldset',
    '#title' => t('Synchronization Settings'),
    '#collapsible' => FALSE,
    '#collapsed' => FALSE,
  );
  $vocabulary_list = taxonomy_get_vocabularies();
  $vocabularies = array();
  foreach($vocabulary_list as $vocabulary) {
    $vocabularies[$vocabulary->vid] = $vocabulary->name;
  }
  $form['sync'][media_theplatform_mpx_NAMESPACE . 'sync_vocabulary'] = array(
    '#type' => 'select',
    '#title' => t('Synchronize vocabulary:'),
    '#options' => $vocabularies,
    '#empty_option' => t('- Select -'),
    '#default_value' => media_theplatform_mpx_variable_get('sync_vocabulary'),
    '#description' => t('Set the Drupal taxonomy which will synchronize with mpx categories. Nodes which use this vocabulary will automatically sync their terms with mpx. Leave empty to to disable vocabulary syncing.'),
  );
  $form['sync'][media_theplatform_mpx_NAMESPACE . 'cron_videos'] = array(
    '#type' => 'checkbox',
    '#title' => t('Sync mpxMedia on Cron'),
    '#default_value' => media_theplatform_mpx_variable_get('cron_videos'),
  );
  $form['sync'][media_theplatform_mpx_NAMESPACE . 'cron_players'] = array(
    '#type' => 'checkbox',
    '#title' => t('Sync mpxPlayers on Cron'),
    '#default_value' => media_theplatform_mpx_variable_get('cron_players'),
  );
  $form['embed_method'] = array(
    '#type' => 'fieldset',
    '#title' => t('Embed Method'),
    '#collapsible' => FALSE,
    '#collapsed' => FALSE,
  );
  $form['embed_method'][media_theplatform_mpx_NAMESPACE . 'embed_method'] = array(
    '#type' => 'radios',
    '#title' => t('Embed Method'),
    '#default_value' => media_theplatform_mpx_variable_get('embed_method', 0),
    '#options' => array(0 => t('HTML with Javascript (Legacy)'), 1 => t('Javascript (Preferred)'), 2 => t('iFrame')),
  );
  return system_settings_form($form);
}

/**
 * Validation for system settings form. If a sync taxonomy was set, checks
 * that either the taxonomy or mpx has no existing terms. If true, it will
 * perform an initial sync. This should be done in a submit handler, but
 * system_settings_form overrides it, so it's done here.
 *
 * @param $form
 * @param $form_state
 */
function media_theplatform_mpx_form_settings_validate($form, &$form_state) {
  $taxonomy = $form_state['values']['media_theplatform_mpx__sync_vocabulary'];
  if($taxonomy && $taxonomy != media_theplatform_mpx_variable_get('sync_vocabulary')) {
    if(!_mpx_taxonomy_check_empty_taxonomy($taxonomy) && !_mpx_taxonomy_check_empty_mpx_categories())
      form_set_error('media_theplatform_mpx__sync_vocabulary', t('The selected taxonomy could not be used because it contains existing terms, and there are existing categories in mpx. You must either choose an empty taxonomy, or have no existing categories defined in mpx.'));
    else {
      media_theplatform_mpx_taxonomy_sync();
    }
  }
}

/******************* mpxPlayers *****************************/

/**
 * Page callback to return all mpx_players and forms.
 */
function media_theplatform_mpx_page_mpx_players() {
  $output = '';
  // Display forms if signin and import_account
  if (media_theplatform_mpx_check_account_settings() && user_access('sync mpx_player')) {
    $form_player_sync = drupal_get_form('media_theplatform_mpx_form_mpx_player_sync');
    $output .= render($form_player_sync);
  }
  $output .= media_theplatform_mpx_get_table_mpx_players();
  return $output;
}

/**
 * Form constructor for mpxPlayer Sync.
 *
 * @ingroup forms
 */
function media_theplatform_mpx_form_mpx_player_sync($form, &$form_state) {
  if (media_theplatform_mpx_get_mpx_player_count() > 0) {
    $collapsed = TRUE;
  }
  else {
    $collapsed = FALSE;
  }
  $form['player_sync'] = array(
    '#type' => 'fieldset',
    '#title' => t('Sync mpxPlayers'),
    '#description' => t('Note: Any mpxPlayer marked as "Disabled" in thePlatform mpx will not be retrieved in the Sync process.'),
    '#collapsible' => TRUE,
    '#collapsed' => $collapsed,
  );
  $form['player_sync']['actions'] = array('#type' => 'actions');
  $form['player_sync']['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Sync mpxPlayers Now'),
  );
  return $form;
}

/**
 * Form submit handler for media_theplatform_mpx_form_mpx_player_sync().
 */
function media_theplatform_mpx_form_mpx_player_sync_submit($form, &$form_state) {
  $import = media_theplatform_mpx_import_all_players('manual');
  if (!$import) {
    drupal_set_message(t('No mpxPlayers were found for your account on thePlatform.  Please create at least 1 mpxPlayer in thePlatform and then click Sync mpxPlayers Now.'), 'error');
  }
  else {
    drupal_set_message(t(':num mpxPlayers returned:', array(':num' => $import['total'])));
    drupal_set_message(t(':num new mpxPlayers created.', array(':num' => $import['inserts'])));
    drupal_set_message(t(':num existing mpxPlayers updated.', array(':num' => $import['updates'])));
    drupal_set_message(t(':num existing mpxPlayers inactivated.', array(':num' => $import['inactives'])));
  }
}

/**
 * Returns a themed table of mpx_player data.
 */
function media_theplatform_mpx_get_table_mpx_players() {
  $header = array(
    // The header gives the table the information it needs in order to make
    // the query calls for ordering. TableSort uses the field information
    // to know what database column to sort by.
    array('data' => t('ID'), 'field' => 'p.player_id'),
    array('data' => NULL),
    array('data' => t('Title'), 'field' => 'p.title'),
    array('data' => t('Description'), 'field' => 'p.description'),
    // array('data' => t('mpx ID'), 'field' => 'p.id'),
    array('data' => t('Status'), 'field' => 'p.status'),
    array('data' => t('First Imported'), 'field' => 'p.created'),
    array('data' => t('Last Updated'), 'field' => 'p.updated'),
  );
  $query = db_select('mpx_player', 'p')
    ->extend('TableSort');
  $query->fields('p');
  $result = $query
    ->orderByHeader($header)
    ->execute();
  $num_rows = $query->countQuery()->execute()->fetchField();
  if ($num_rows == 0) {
    return '<div class="message">' . t('No mpxPlayers have been imported.') . '</div>';
  }
  foreach ($result as $player) {
    if ($player->fid == media_theplatform_mpx_variable_get('default_player_fid')) {
      $default = '[default]';
    }
    else {
      $default = NULL;
    }
    $rows[] = array(
      $player->player_id,
      $default,
      l($player->title, MPX_PATH_FILE . '/' . $player->fid),
      $player->description,
      // $player->id,
      $player->status,
      format_date($player->created, 'short'),
      format_date($player->updated, 'short'),
    );
  }
  return theme('table', array('header' => $header, 'rows' => $rows));
}


/******************** mpxMedia *******************************/

/**
 * Page callback - display all mpx Video media and forms.
 */
function media_theplatform_mpx_page_mpx_videos() {
  $output = '';
  // If no mpxPlayers, you cant do anything with mpxMedia.
  if (!media_theplatform_mpx_get_mpx_player_count()) {
    $output .= t('No mpxPlayers have been imported.');
    if (user_access('sync mpx_player')) {
      $output .= ' ' . l(t('Sync mpxPlayers.'), MPX_PATH_PLAYER);
    }
    else {
      $output .= ' ' . t('Please contact your System Administrator.');
    }
    return $output;
  }
  if (media_theplatform_mpx_check_account_settings() && user_access('sync mpx_video')) {
    $form_video_sync = drupal_get_form('media_theplatform_mpx_form_mpx_video_sync');
    $output .= render($form_video_sync);
  }
  $output .= media_theplatform_mpx_get_table_mpx_videos();
  return $output;
}

/**
 * Form constructor for mpx_video Sync.
 *
 * @ingroup forms
 */
function media_theplatform_mpx_form_mpx_video_sync($form, &$form_state) {
  $import_account = media_theplatform_mpx_variable_get('import_account');
  $player_select = media_theplatform_mpx_get_players_select($import_account);
  if (media_theplatform_mpx_get_mpx_video_count() > 0) {
    $collapsed = TRUE;
  }
  else {
    $collapsed = FALSE;
  }
  $form['video_sync'] = array(
    '#type' => 'fieldset',
    '#title' => t('Sync mpxMedia'),
    '#description' => t('Note: Any mpxMedia which has been unpublished or deleted from thePlatform mpx will be set to "Inactive".'),
    '#collapsible' => TRUE,
    '#collapsed' => $collapsed,
  );
  $form['video_sync'][media_theplatform_mpx_NAMESPACE . 'default_player_fid'] = array(
    '#type' => 'select',
    '#title' => t('Import new mpxMedia with mpxPlayer:'),
    '#options' => $player_select,
    '#empty_option' => t('- Select -'),
    '#required' => TRUE,
    '#default_value' => media_theplatform_mpx_variable_get('default_player_fid'),
  );
  $form['video_sync']['actions'] = array('#type' => 'actions');
  $form['video_sync']['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Sync mpxMedia Now'),
  );
  return $form;
}

/**
 * Form submission handler for media_theplatform_mpx_form_mpx_video_sync().
 */
function media_theplatform_mpx_form_mpx_video_sync_submit($form, &$form_state) {

  // Set Default mpxPlayer variable.
  $default_player_fid = $form_state['values'][media_theplatform_mpx_NAMESPACE . 'default_player_fid'];
  media_theplatform_mpx_variable_set('default_player_fid', $default_player_fid);

  // Import videos.
  $import = media_theplatform_mpx_import_all_videos('manual');
  if ($import) {
    drupal_set_message(t(':num new mpxMedia created.', array(':num' => $import['inserted'])));
    drupal_set_message(t(':num existing mpxMedia updated.', array(':num' => $import['updated'])));
    drupal_set_message(t(':num existing mpxMedia unpublished.', array(':num' => $import['unpublished'])));
    drupal_set_message(t(':num existing mpxMedia deleted.', array(':num' => $import['deleted'])));
  }
}

/**
 * Returns themed table of mpx_video data.
 */
function media_theplatform_mpx_get_table_mpx_videos() {
  $header = array(
    // The header gives the table the information it needs in order to make
    // the query calls for ordering. TableSort uses the field information
    // to know what database column to sort by.
    array('data' => t('ID'), 'field' => 'v.video_id'),
    array('data' => NULL),
    array('data' => t('Title'), 'field' => 'v.title'),
    array('data' => t('Description'), 'field' => 'v.description'),
    // array('data' => t('mpx ID (Guid)'), 'field' => 'v.guid'),
    array('data' => t('Status'), 'field' => 'v.status'),
    array('data' => t('First Imported'), 'field' => 'v.created'),
    array('data' => t('Last Updated'), 'field' => 'v.updated'),
  );
  $query = db_select('mpx_video', 'v')
    ->extend('TableSort');
  $query->fields('v');
  $result = $query
    ->orderByHeader($header)
    ->execute();
  $num_rows = $query->countQuery()->execute()->fetchField();
  if ($num_rows == 0) {
    return '<div class="message">' . t('No mpxMedia has been imported.') . '</div>';
  }
  foreach ($result as $video) {
    $file = file_load($video->fid);
    $thumbnail = media_get_thumbnail_preview($file);
    $rows[] = array(
      $video->video_id,
      l(drupal_render($thumbnail), MPX_PATH_FILE . '/' . $video->fid, array('html' => TRUE, 'attributes' => array('class' => 'mpxmedia'))),
      $video->title,
      $video->description,
      // $video->guid,
      $video->status,
      format_date($video->created, 'short'),
      format_date($video->updated, 'short'),
    );
  }
  return theme('table', array('header' => $header, 'rows' => $rows));
}


/******************* File field mapping *****************************/

/**
 * Form for mapping file entity fields to mpx fields.
 * @param $form
 * @param $form_state
 * @return mixed
 */
function media_theplatform_mpx_page_mpx_file_mapping_form($form, $form_state) {
  drupal_set_message(t('<b>Please note:</b> currently, this module only provides support for mapping between fields with single values and matching data types. Choices are limited to fields which match these conditions. In the future, additional work will be done to offer more complex field type mappings.<br /><br />Additionally, if multiple fields are mapped to a single field on either side, the last field to be processed will take precedence, overwriting the former value.'), 'warning');
  $instances = field_info_instances('file', 'video');
  $drupal_fields = array();
  $mpx_fields = _mpx_fields_get_mpx_fields();
  $default_values = unserialize(media_theplatform_mpx_variable_get('file_field_map'));
  foreach($instances as $id => $val) {
    $drupal_fields[$id] = $val['label'];
  }
  $form['fields'] = array(
    '#tree' => TRUE,
    '#theme' => 'media_theplatform_mpx_file_map_table'
  );
  foreach($drupal_fields as $drupal_field_id => $drupal_field_label) {
    $field_options = array();
    $field_info = field_info_field($drupal_field_id);
    // Taxonomies receive special handling: we're only mapping taxonomy fields
    // which use the vocabulary configured to be synced and can accept unlimited values
    // because MPX categories accept unlimited values.
    if($field_info['type'] == 'taxonomy_term_reference' && $field_info['cardinality'] == "-1") {
      if($sync_vid = media_theplatform_mpx_variable_get('sync_vocabulary', false)) {
        $vocabulary_name = $field_info['settings']['allowed_values'][0]['vocabulary'];
        $vid = db_select('taxonomy_vocabulary', 't')
          ->fields('t', array('vid'))
          ->condition('machine_name', $vocabulary_name, '=')
          ->execute()
          ->fetchField();
        if($vid === $sync_vid) {
          $field_options['categories'] = 'Categories';
        }
      }
    } else {
      // All other field types
      foreach($mpx_fields as $mpx_field_id => $mpx_field_object) {
        if(_mpx_fields_check_compatibility($drupal_field_id, $mpx_field_object))
          $field_options[$mpx_field_id] = $mpx_field_object->title;
      }
    }

    if(count($field_options)) {
      $form['fields'][$drupal_field_id]['drupal_field'] = array(
        '#type' => 'markup',
        '#markup' => $drupal_field_label,
      );

      $form['fields'][$drupal_field_id]['mpx_field'] = array(
        '#type' => 'select',
        '#title' => '',
        '#description' => '',
        '#options' => $field_options,
        '#default_value' => isset($default_values[$drupal_field_id]) ? $default_values[$drupal_field_id] : '',
        '#empty_option' => t('- Select -'),
      );
    }
  }
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );
  return $form;
}

/**
 * Submit handler for file field mapping form
 * @param $form
 * @param $form_state
 */
function media_theplatform_mpx_page_mpx_file_mapping_form_submit($form, $form_state) {
  $fields = array();
  foreach($form_state['values']['fields'] as $id => $val) {
    if(strpos($id, 'field_') === 0 && strlen($val['mpx_field']))
      $fields[$id] = $val['mpx_field'];
  }

  media_theplatform_mpx_variable_set('file_field_map', serialize($fields));
}