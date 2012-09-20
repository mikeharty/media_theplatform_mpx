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
  if (media_theplatform_mpx_variable_get('token')) {
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
  $username = $form_state['values']['theplatform_username'];
  $pass = $form_state['values']['theplatform_password'];
  if (media_theplatform_mpx_signin($username, $pass)) {
    media_theplatform_mpx_variable_set('username', $username);
    media_theplatform_mpx_variable_set('password', $pass);
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
    '#description' => t('Set the account from which to import Players and Feeds from.'),
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
  $feed_url = 'http://access.auth.theplatform.com/data/Account?schema=1.3.0&form=json&byDisabled=false&byTitle=' . $import_account . '&token=' . media_theplatform_mpx_variable_get('token');
  $result = drupal_http_request($feed_url);
  $result_data = drupal_json_decode($result->data);
  $mpx_account_pid = $result_data['entries'][0]['placcount$pid'];
  // Set account_pid variable.
  media_theplatform_mpx_variable_set('account_pid', $mpx_account_pid);

  // Import all players for this account.
  $import = media_theplatform_mpx_import_all_players('manual');
  drupal_set_message(t('Import account set.') . ' ' . l(t('All Players imported from the Import account.'), MPX_PATH_PLAYER));
}

/**
 * Returns TRUE if token and import_account set.  If not, drupal_set_message and returns FALSE.
 */
function media_theplatform_mpx_check_account_settings() {
  if (!media_theplatform_mpx_variable_get('token') || !media_theplatform_mpx_variable_get('import_account')) {
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
  $form['player_settings'][media_theplatform_mpx_NAMESPACE . 'runtimes'] = array(
    '#type' => 'select',
    '#title' => t('Select Site-wide Runtimes:'),
    '#description' => t('Select how all mpxPlayers should be rendered.'),
    '#options' => media_theplatform_mpx_get_runtimes_select(),
    '#required' => TRUE,
    '#default_value' => media_theplatform_mpx_variable_get('runtimes'),
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
 * Form constructor for mpx Player Sync.
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
    drupal_set_message(t(':num mpx Players returned:', array(':num' => $import['total'])));
    drupal_set_message(t(':num new mpx Players created.', array(':num' => $import['inserts'])));
    drupal_set_message(t(':num existing mpx Players updated.', array(':num' => $import['updates'])));
    drupal_set_message(t(':num existing mpx Players inactivated.', array(':num' => $import['inactives'])));
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
      l($player->title, 'media/' . $player->fid),
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
  if (media_theplatform_mpx_check_account_settings()) {
    $settings_access = user_access('administer mpx_video settings');
    $sync_access = user_access('sync mpx_video');
    if ($settings_access || $sync_access) {
      $output .= media_theplatform_mpx_get_mpx_video_forms($settings_access, $sync_access);
    }
  }
  $output .= media_theplatform_mpx_get_table_mpx_videos();
  return $output;
}

/**
 * Return rendered mpx_video forms based on signIn and user_access.
 */
function media_theplatform_mpx_get_mpx_video_forms($settings_access, $sync_access) {
  // Request all feeds for the account.
  $feeds = media_theplatform_mpx_get_feeds_select();
  // If no results from thePlatform, prompt user to create one in thePlatform.
  if (!$feeds) {
    drupal_set_message(t('There are no Feeds configured in your account on thePlatform.  Please create a Feed in thePlatform and refresh this page in order to set a Feed to import from.'), 'error');
    return FALSE;
  }
  $output = '';
  $default_feed_id = media_theplatform_mpx_variable_get('feed');
  $default_player_fid = media_theplatform_mpx_variable_get('default_player_fid');
  $import_account = media_theplatform_mpx_variable_get('import_account');
  $is_valid_default = media_theplatform_mpx_is_valid_player_for_account($default_player_fid, $import_account);

  // If the current Feed id is not in the Feed results, warning msg.
  if ($default_feed_id && !array_key_exists($default_feed_id, $feeds)) {
    $bad_feed = TRUE;
    $output .= '<div class="message">' . (t('The current Import Feed (Public ID = :name) was not found in thePlatform; it may have been disabled or deleted.', array(':name' => $default_feed_id))) . '</div>';
  }
  // If settings access, display mpx_video settings form.
  if ($settings_access) {
    $form_video_settings = drupal_get_form('media_theplatform_mpx_form_mpx_video_settings', $feeds, $is_valid_default);
    $output .= render($form_video_settings);
  }
  // If access and ok feed, display mpx_video sync form.
  if ($default_feed_id && ($settings_access || $sync_access) && !isset($bad_feed) && $is_valid_default) {
    $form_video_sync = drupal_get_form('media_theplatform_mpx_form_mpx_video_sync');
    $output .= render($form_video_sync);
  }
  return $output;
}

/**
 * Form constructor for mpx_video Settings.
 *
 * @param array $feeds
 *   Array of feeds from thePlatform.
 * @param boolean $is_valid_default
 *   If the default player fid is valid for the import account.
 *
 * @see media_theplatform_mpx_get_mpx_video_forms()
 *
 * @ingroup forms
 */
function media_theplatform_mpx_form_mpx_video_settings($form, &$form_state, $feeds, $is_valid_default) {
  $default_feed_id = media_theplatform_mpx_variable_get('feed');
  $import_account = media_theplatform_mpx_variable_get('import_account');
  $player_select = media_theplatform_mpx_get_players_select($import_account);

  // If feed is set and valid, and default player is valid, form is collapsed by default.
  if ($default_feed_id && array_key_exists($default_feed_id, $feeds) && $is_valid_default) {
    $collapsed = TRUE;
  }
  else {
    $collapsed = FALSE;
  }
  $form['video_settings'] = array(
    '#type' => 'fieldset',
    '#title' => t('mpxMedia Settings'),
    '#collapsible' => TRUE,
    '#collapsed' => $collapsed,
  );
  $form['video_settings'][media_theplatform_mpx_NAMESPACE . 'feed'] = array(
    '#type' => 'select',
    '#title' => t('Sync mpxMedia From:'),
    '#options' => $feeds,
    '#empty_option' => t('- Select -'),
    '#required' => TRUE,
    '#default_value' => media_theplatform_mpx_variable_get('feed'),
    '#description' => t('Select your mpx Feed to use for importing mpxMedia into the Drupal Media Library.') . '<p>' . t('Note: mpx Feeds which have been marked as "Disabled" in thePlatform mpx will not appear in this dropdown.') . '</p>',
  );

  $form['video_settings'][media_theplatform_mpx_NAMESPACE . 'default_player_fid'] = array(
    '#type' => 'select',
    '#title' => t('Select Default mpxPlayer:'),
    '#description' => t('This will be the mpxPlayer used for any imported mpxMedia.'),
    '#options' => $player_select,
    '#empty_option' => t('- Select -'),
    '#required' => TRUE,
    '#default_value' => media_theplatform_mpx_variable_get('default_player_fid'),
  );
  $form['video_settings']['actions'] = array('#type' => 'actions');
  $form['video_settings']['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save Settings'),
  );
  return $form;
}

/**
 * Form submission handler for media_theplatform_mpx_form_mpx_video_settings().
 */
function media_theplatform_mpx_form_mpx_video_settings_submit($form, &$form_state) {
  $feed = $form_state['values'][media_theplatform_mpx_NAMESPACE . 'feed'];
  $default_player_fid = $form_state['values'][media_theplatform_mpx_NAMESPACE . 'default_player_fid'];
  global $user;
  $log = array(
    'uid' => $user->uid,
    'type' => 'settings',
    'type_id' => NULL,
    'action' => 'video',
    'details' => 'new feed = ' . $feed . ', default player fid = ' . $default_player_fid . ' | old feed = ' . media_theplatform_mpx_variable_get('feed'),
  );
  media_theplatform_mpx_insert_log($log);
  media_theplatform_mpx_variable_set('feed', $feed);
  media_theplatform_mpx_variable_set('default_player_fid', $default_player_fid);
  drupal_set_message(t('Import Feed set.  Click "Sync mpxMedia Now" to import mpxMedia from this Feed.'));
}

/**
 * Form constructor for mpx_video Sync.
 *
 * @ingroup forms
 */
function media_theplatform_mpx_form_mpx_video_sync($form, &$form_state) {
  if (media_theplatform_mpx_get_mpx_video_count() > 0) {
    $collapsed = TRUE;
  }
  else {
    $collapsed = FALSE;
  }
  $form['video_sync'] = array(
    '#type' => 'fieldset',
    '#title' => t('Sync mpxMedia'),
    '#description' => t('Note: Any mpxMedia which is no longer included in the Import Feed will be set to "Inactive".  mpxMedia must have a Title and Thumbnail set in thePlatform.'),
    '#collapsible' => TRUE,
    '#collapsed' => $collapsed,
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
  if (!media_theplatform_mpx_variable_get('feed')) {
    drupal_set_message(t('You must select a Feed to import from.'), 'error');
    return;
  }
  $import = media_theplatform_mpx_import_all_videos('manual');
  if (!$import) {
    drupal_set_message(t('No mpxMedia was found for the selected import Feed on thePlatform.  Please create add at least 1 mpxMedia to the Feed in thePlatform and then click Sync mpxMedia Now.'), 'error');
  }
  elseif ($import == 'no thumbnails') {
    drupal_set_message(t('One or more of the mpxMedia in the selected Feed does not contain valid thumbnail data.  Please correct in thePlatform and then click Sync mpxMedia Now.'), 'error');
  }
  else {
    drupal_set_message(t(':num mpxMedia returned:', array(':num' => $import['total'])));
    drupal_set_message(t(':num new mpxMedia created.', array(':num' => $import['inserts'])));
    drupal_set_message(t(':num existing mpxMedia updated.', array(':num' => $import['updates'])));
    drupal_set_message(t(':num existing mpxMedia inactivated.', array(':num' => $import['inactives'])));
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
      l(drupal_render($thumbnail), 'media/' . $video->fid, array('html' => TRUE)),
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
