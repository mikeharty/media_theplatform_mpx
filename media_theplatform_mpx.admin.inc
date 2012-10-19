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
      $cron_form = drupal_get_form('media_theplatform_mpx_form_cron_settings');
    }
    $form_account = drupal_get_form('media_theplatform_mpx_form_account_theplatform', $account_list);
    $output .= render($form_account);
    $output .= render($cron_form);
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
function media_theplatform_mpx_form_cron_settings() {
  $form['cron'] = array(
    '#type' => 'fieldset',
    '#title' => t('Cron Settings'),
    '#collapsible' => FALSE,
    '#collapsed' => FALSE,
  );
  $form['cron'][media_theplatform_mpx_NAMESPACE . 'cron_videos'] = array(
    '#type' => 'checkbox',
    '#title' => t('Sync mpxMedia on Cron'),
    '#default_value' => media_theplatform_mpx_variable_get('cron_videos'),
  );
  $form['cron'][media_theplatform_mpx_NAMESPACE . 'cron_players'] = array(
    '#type' => 'checkbox',
    '#title' => t('Sync mpxPlayers on Cron'),
    '#default_value' => media_theplatform_mpx_variable_get('cron_players'),
  );
  return system_settings_form($form);
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
