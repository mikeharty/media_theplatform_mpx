<?php

/**
 * @file
 * Functions for callbacks for items in media_theplatform_mpx_menu().
 */


/********* thePlatform signIn / Account ***********************/

/**
 * Page callback to return signin / account forms.
 */
function media_theplatform_mpx_page_account() {
  $form_signin = drupal_get_form('media_theplatform_mpx_form_signin_theplatform');
  $output = render($form_signin);

  // If we have signin, display account list.
  if (media_theplatform_mpx_variable_get('token')) {
    $account_list = media_theplatform_mpx_get_accounts_select();
    $import_account = media_theplatform_mpx_variable_get('import_account');
    // If current account doesn't exist, error msg.
    if ($import_account && !array_key_exists($import_account, $account_list)) {
      $output .= '<div class="message">' . t('The current Import Account (:name) was not found in thePlatform; it may have been disabled or deleted.  Please select a new Account to use as the Import Account.', array(':name' => rawurldecode($import_account))) . '</div>';
    }
    $form_account = drupal_get_form('media_theplatform_mpx_form_account_theplatform', $account_list);
    $output .= render($form_account);
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

/******************* mpx Players *****************************/

/**
 * Page callback to return all mpx_players and forms.
 */
function media_theplatform_mpx_page_mpx_players() {
  $output = '';

  // Display forms if signin and import_account
  if (media_theplatform_mpx_check_account_settings()) {
    // Render settings form.
    if (user_access('administer mpx_player settings')) {
      $form_player_settings = drupal_get_form('media_theplatform_mpx_form_mpx_player_settings');
      $output .= render($form_player_settings);
      $sync_access = TRUE;
    }
    // Render sync form.
    if (isset($sync_access) || user_access('sync mpx_player')) {
      $form_player_sync = drupal_get_form('media_theplatform_mpx_form_mpx_player_sync');
      $output .= render($form_player_sync);
    }
  }

  $output .= media_theplatform_mpx_get_table_mpx_players();
  return $output;
}

/**
 * Form constructor for mpx Player Settings.
 *
 * @ingroup forms
 */
function media_theplatform_mpx_form_mpx_player_settings($form, &$form_state) {
  $player_select = media_theplatform_mpx_get_players_select();
  if (!$player_select) {
    return FALSE;
  }
  $default = media_theplatform_mpx_variable_get('default_player_fid');
  if ($default && array_key_exists($default, $player_select)) {
    $collapsed = TRUE;
  }
  else {
    $collapsed = FALSE;
  }
  $form['player_settings'] = array(
    '#type' => 'fieldset',
    '#title' => t('mpxPlayer Settings'),
    '#collapsible' => TRUE,
    '#collapsed' => $collapsed,
  );
  $form['player_settings'][media_theplatform_mpx_NAMESPACE . 'default_player_fid'] = array(
    '#type' => 'select',
    '#title' => t('Select Default mpxPlayer:'),
    '#description' => t('This will be the mpxPlayer used for any imported mpxMedia.') . '<p>' . t('Note: If you change the Default mpxPlayer, you must clear the Drupal cache in order to update the mpxMedia.') . '</p>',
    '#options' => $player_select,
    '#empty_option' => t('- Select -'),
    '#required' => TRUE,
    '#default_value' => media_theplatform_mpx_variable_get('default_player_fid'),
  );
  $form['player_settings'][media_theplatform_mpx_NAMESPACE . 'runtimes'] = array(
    '#type' => 'select',
    '#title' => t('Select Site-wide Runtimes:'),
    '#description' => t('Select how all mpxPlayers should be rendered.'),
    '#options' => media_theplatform_mpx_get_runtimes_select(),
    '#required' => TRUE,
    '#default_value' => media_theplatform_mpx_variable_get('runtimes'),
  );
  $form['player_settings'][media_theplatform_mpx_NAMESPACE . 'cron_players'] = array(
    '#type' => 'checkbox',
    '#title' => t('Sync mpxPlayers on Cron'),
    '#default_value' => media_theplatform_mpx_variable_get('cron_players'),
  );
  $form['player_settings']['actions'] = array('#type' => 'actions');
  $form['player_settings']['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save Settings'),
  );
  return $form;
}

/**
 * Form submit handler for media_theplatform_mpx_form_mpx_player_settings().
 */
function media_theplatform_mpx_form_mpx_player_settings_submit($form, &$form_state) {
  $default_player = $form_state['values'][media_theplatform_mpx_NAMESPACE . 'default_player_fid'];
  $runtimes = $form_state['values'][media_theplatform_mpx_NAMESPACE . 'runtimes'];
  $cron_players = $form_state['values'][media_theplatform_mpx_NAMESPACE . 'cron_players'];
  global $user;
  $log = array(
    'uid' => $user->uid,
    'type' => 'settings',
    'type_id' => NULL,
    'action' => 'player',
    'details' => 'player new default = ' . $default_player . ' | player old default = ' . media_theplatform_mpx_variable_get('default_player_fid') . ' | player runtimes = ' . $runtimes . ' | cron players = ' . $cron_players,
  );
  media_theplatform_mpx_insert_log($log);
  media_theplatform_mpx_variable_set('default_player_fid', $default_player);
  media_theplatform_mpx_variable_set('runtimes', $runtimes);
  media_theplatform_mpx_variable_set('cron_players', $cron_players);
  drupal_set_message(t('Player Settings saved. You must clear the Drupal cache for updates to the Default mpxPlayer to appear.'));
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

/******************** mpx Videos *******************************/

/**
 * Page callback - display all mpx Video media and forms.
 */
function media_theplatform_mpx_page_mpx_videos() {
  $output = '';
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
  // If the current Feed id is not in the Feed results, warning msg.
  if ($default_feed_id && !array_key_exists($default_feed_id, $feeds)) {
    $bad_feed = TRUE;
    $output .= '<div class="message">' . (t('The current Import Feed (Public ID = :name) was not found in thePlatform; it may have been disabled or deleted.', array(':name' => $default_feed_id))) . '</div>';
  }
  // If settings access, display mpx_video settings form.
  if ($settings_access) {
    $form_video_settings = drupal_get_form('media_theplatform_mpx_form_mpx_video_settings', $feeds);
    $output .= render($form_video_settings);
  }
  // If access and ok feed, display mpx_video sync form.
  if ($default_feed_id && ($settings_access || $sync_access) && !isset($bad_feed)) {
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
 *
 * @see media_theplatform_mpx_get_mpx_video_forms()
 *
 * @ingroup forms
 */
function media_theplatform_mpx_form_mpx_video_settings($form, &$form_state, $feeds) {
  $default_feed_id = media_theplatform_mpx_variable_get('feed');
  if ($default_feed_id && array_key_exists($default_feed_id, $feeds)) {
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
  $form['video_settings'][media_theplatform_mpx_NAMESPACE . 'cron_videos'] = array(
    '#type' => 'checkbox',
    '#title' => t('Sync mpxMedia on Cron'),
    '#default_value' => media_theplatform_mpx_variable_get('cron_videos'),
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
  $cron_videos = $form_state['values'][media_theplatform_mpx_NAMESPACE . 'cron_videos'];

  global $user;
  $log = array(
    'uid' => $user->uid,
    'type' => 'settings',
    'type_id' => NULL,
    'action' => 'video',
    'details' => 'new value = ' . $feed . ' | old value = ' . media_theplatform_mpx_variable_get('feed') . ' | cron videos = ' . $cron_videos,
  );
  media_theplatform_mpx_insert_log($log);
  media_theplatform_mpx_variable_set('feed', $feed);
  media_theplatform_mpx_variable_set('cron_videos', $cron_videos);
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
  // If there's no valid default player selected, error msg.
  if (!media_theplatform_mpx_is_valid_default_player()) {
    if (user_access('administer player settings')) {
      drupal_set_message(t('A Default mpxPlayer must be set to view mpxMedia.') . ' ' . l('Configure mpxPlayer Settings', MPX_PATH_PLAYER), 'error');      
    }
    else {
      drupal_set_message(t('A Default mpxPlayer must be set to view mpxMedia.') . ' ' . t('Please contact your System Administrator.'), 'error');
    }
  }
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

/********************* Media browser forms ****************************/


/**
 * Form constructor for mpx Media Browser.
 *
 * @param string $browser_type
 *   Valid values are 'videos' or 'players'.
 *
 * @see media_theplatform_mpx_media_browser_plugin_view()
 * @ingroup forms
 */
function media_theplatform_mpx_form_media_browser($form, &$form_state = array(), $browser_type = NULL) {
  // Need this for media_browser_build_media_item.
  module_load_include('inc', 'media', 'includes/media.browser');
  $form['videos']['#prefix'] = '<div id="container"><div id="scrollbox"><ul id="media-browser-library-list" class="media-list-thumbnails">';
  $form['videos']['#suffix'] = '</ul><div id="status"></div></div></div>';
  $files = array();
  if ($browser_type == 'videos') {
    $list = media_theplatform_mpx_get_all_mpx_videos();
    $label = 'mpxMedia';
  }
  elseif ($browser_type == 'players') {
    $list = media_theplatform_mpx_get_all_mpx_players();
    $label = 'mpxPlayers';
  }
  else {
    return FALSE;
  }
  if (count($list) == 0) {
    $form['no-items'] = array(
      '#markup' => t('No :type have been imported.', array(':type' => $label)),
    );
    return $form;
  }

  foreach ($list as $item) {
    $file = file_load($item->fid);
    media_browser_build_media_item($file);
    $file->preview = l($file->preview, 'media/browser', array(
      'html' => TRUE,
      'attributes' => array(
        'data-uri' => $file->uri,
      ),
      'query' => array('render' => 'media-popup', 'uri' => $file->uri),
    ));
    $form['videos'][$file->uri] = array(
      '#markup' => $file->preview,
      '#prefix' => '<li>',
      '#suffix' => '</li>',
    );
    $files[$file->uri] = $file;
  }
  $form['submitted-video'] = array(
    '#type' => 'hidden',
    '#default_value' => FALSE,
  );
  // Add the files to JS so that they are accessible inside the browser.
  drupal_add_js(array('media' => array('files' => $files)), 'setting');
  // Add media_theplatform_mpx browser javascript and CSS.
  drupal_add_js(drupal_get_path('module', 'media_theplatform_mpx') . '/js/media_theplatform_mpx.browser.js');
  // Standard Media module files.
  drupal_add_js(drupal_get_path('module', 'media') . '/js/plugins/media.library.js');
  drupal_add_css(drupal_get_path('module', 'media') . '/js/plugins/media.library.css');

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Select'),
    '#attributes' => array('class' => array('mpx-submit')),
  );
  return $form;
}

/**
 * Form submission handler for media_theplatform_mpx_form_media_browser().
 */
function media_theplatform_mpx_form_media_browser_submit($form, &$form_state) {
  $embed_code = $form_state['values']['submitted-video'];
  $fid = db_query("SELECT fid FROM {file_managed} WHERE uri=:uri", array(':uri' => $embed_code))->fetchField();
  // This stops page reload inside the media browser iframe.
  $form_state['redirect'] = array('media/browser', array(
    'query' => array(
      'render' => 'media-popup',
      'fid' => $fid,
    )));
}
