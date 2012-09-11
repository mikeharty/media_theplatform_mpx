<?php
/**
 * @file
 * functions for Players.
 */

/**
 * Requests all mpx Players for specified thePlatform account.
 *
 * - Returns array of Players' data indexed by Player id if there are Players.
 * - Returns FALSE if no Players exist in mpx account.
 * - Returns error msg if no mpx_token variable.
 */
function media_theplatform_mpx_get_players_from_theplatform() {
  // Check for the signIn token and account.
  $mpx_token = media_theplatform_mpx_variable_get('token');
  $mpx_account = media_theplatform_mpx_variable_get('import_account');
  if (!$mpx_token || !$mpx_account) {
    return t('There was an error with your request.');
  }

  global $user;
  // @todo - do some kind of check to bring back a max # of records?
  // Get the list of players from thePlatform.
  $player_url = 'http://data.player.theplatform.com/player/data/Player?schema=1.3.0&form=json&token=' . $mpx_token . '&account=' . $mpx_account;

  $result = drupal_http_request($player_url);
  $result_data = drupal_json_decode($result->data);
  if ($result_data['entryCount'] == 0) {
    $log = array(
      'uid' => $user->uid,
      'type' => 'request',
      'type_id' => NULL,
      'action' => 'player',
      'details' => '0 players returned.',
    );
    media_theplatform_mpx_insert_log($log);
    return FALSE;
  }
  foreach ($result_data['entries'] as $player) {
    // We only want Players which are not disabled.
    if (!$player['plplayer$disabled']) {
      $players[] = array(
        'id' => str_replace(media_theplatform_mpx_ID_PREFIX, '', $player['id']),
        'guid' => $player['guid'],
        'title' => $player['title'],
        'description' => $player['description'],
        'pid' => $player['plplayer$pid'],
      );
    }
  }
  $log = array(
    'uid' => $user->uid,
    'type' => 'request',
    'type_id' => NULL,
    'action' => 'player',
    'details' => count($players) . ' players returned.',
  );
  media_theplatform_mpx_insert_log($log);
  return $players;
}

/**
 * Returns array of options for the runtimes variable.
 */
function media_theplatform_mpx_get_runtimes_select() {
  return array(
    'Flash,HTML5' => 'Flash,HTML5',
    'HTML5,Flash' => 'HTML5,Flash',
  );
}

/**
 * Returns array of Player fid's and Titles.
 */
function media_theplatform_mpx_get_players_select() {
  // Retrieve players from mpx_player.
  $result = db_select('mpx_player', 'p')
    ->fields('p', array('fid', 'title'))
    ->execute();
  // Index by file fid.
  while ($record = $result->fetchAssoc()) {
    $players[$record['fid']] = $record['title'];
  }
  return $players;
}

/**
 * Returns URL string of a player for given $pid.
 */
function media_theplatform_mpx_get_player_url($pid) {
  return 'http://player.theplatform.com/p/' . media_theplatform_mpx_get_account_pid() . '/' . $pid;
}

/**
 * Returns HTML from a Player's URL.
 *
 * @param String $pid
 *   A Player's Pid.
 * @param String $type
 *   Either 'head' or 'body'.
 *
 * @return String
 *   HTML requested.
 */
function media_theplatform_mpx_get_player_html($pid, $type = 'head') {
  $url = media_theplatform_mpx_get_player_url($pid) . '/' . $type;
  $result = drupal_http_request($url);
  return $result->data;
}

/**
 * Imports all Players into Media Library.
 *
 * @param String $type
 *   Import type. Possible values 'cron' or 'manual', for sync.
 *
 * @return Array
 *   $data['total'] - # of players retrieved
 *   $data['num_inserts'] - # of players added to mpx_player table
 *   $data['num_updates'] - # of players updated
 *   $data['num_inactives'] - # of players changed from active to inactive
 */
function media_theplatform_mpx_import_all_players($type = NULL) {

  // Clicked on Players Sync form.
  if ($type == 'manual') {
    global $user;
    $uid = $user->uid;
  }
  else {
    $uid = 0;
  }
  $log = array(
    'uid' => $uid,
    'type' => 'player',
    'type_id' => NULL,
    'action' => 'import',
    'details' => $type,
  );
  media_theplatform_mpx_insert_log($log);

  // Retrieve list of players.
  $players = media_theplatform_mpx_get_players_from_theplatform();
  if (!$players) {
    return FALSE;
  }

  // Initalize our counters.
  $num_inserts = 0;
  $num_updates = 0;
  $num_inactives = 0;
  $incoming = array();

  // Loop through players retrieved.
  foreach ($players as $player) {
    // Keep track of the incoming id.
    $incoming[] = $player['id'];
    // Import this player.
    $op = media_theplatform_mpx_import_player($player);
    if ($op == 'insert') {
      $num_inserts++;
    }
    elseif ($op == 'update') {
      $num_updates++;
    }
  }
  $num_inactives = 0;

  // Find all mpx_player records NOT in $incoming with status = 1.
  $inactives = db_select('mpx_player', 'p')
    ->fields('p', array('player_id', 'fid', 'id'))
    ->condition('id', $incoming, 'NOT IN')
    ->condition('status', 1, '=')
    ->execute();

  global $user;

  // Loop through results:
  while ($record = $inactives->fetchAssoc()) {
    // Set status to inactive.
    $inactive = db_update('mpx_player')
      ->fields(array('status' => 0))
      ->condition('player_id', $record['player_id'], '=')
      ->execute();

    // Log inactive update.
    $log = array(
      'uid' => $user->uid,
      'type' => 'player',
      'type_id' => $record['player_id'],
      'action' => 'inactive',
      'details' => NULL,
    );
    media_theplatform_mpx_insert_log($log);
    $num_inactives++;
  }

  // Return counters as an array.
  return array(
    'total' => count($players),
    'inserts' => $num_inserts,
    'updates' => $num_updates,
    'inactives' => $num_inactives,
  );
}

/**
 * Updates or inserts given Player within Media Library.
 *
 * @param array $player
 *   Record of Player data requested from thePlatform
 *
 * @return String
 *   Returns output of media_theplatform_mpx_update_player() or media_theplatform_mpx_insert_player()
 */
function media_theplatform_mpx_import_player($player) {
  // Check if fid exists in files table for URI = mpx://p/ID.
  $id = $player['id'];
  $uri = 'mpx://p/' . $id;
  $fid = db_query("SELECT fid FROM {file_managed} WHERE uri=:uri", array(':uri' => $uri))->fetchField();

  // If fid exists:
  if ($fid) {
    // Check if record already exists in mpx_player.
    $imported = db_query("SELECT fid FROM {mpx_player} WHERE id=:id", array(':id' => $id))->fetchField();
    // If mpx_player record exists, then update record.
    if ($imported) {
      return media_theplatform_mpx_update_player($player, $fid);
    }
    // Else insert new mpx_player record with existing $fid.
    else {
      return media_theplatform_mpx_insert_player($player, $fid);
    }
  }
  // Else fid doesn't exist:
  else {
    // Create new mpx_player and create new file.
    return media_theplatform_mpx_insert_player($player, NULL);
  }
}

/**
 * Inserts given Player and File into Media Library.
 *
 * @param array $player
 *   Record of Player data requested from thePlatform
 * @param int $fid
 *   File fid of Player's File in file_managed if it already exists
 *   NULL if it doesn't exist
 *
 * @return String
 *   Returns 'insert' for counters in media_theplatform_mpx_import_all_players()
 */
function media_theplatform_mpx_insert_player($player, $fid = NULL) {
  $timestamp = REQUEST_TIME;

  // If file doesn't exist, write it to file_managed.
  if (!$fid) {
    // Build embed string to create file:
    // "p" is for player.
    $embed_code = 'mpx://p/' . $player['id'];
    // Create the file.
    $provider = media_internet_get_provider($embed_code);
    $file = $provider->save();
    $fid = $file->fid;
    $details = 'new fid = ' . $fid;
  }
  else {
    $details = 'existing fid = ' . $fid;
  }

  // Insert record into mpx_player.
  $player_id = db_insert('mpx_player')
    ->fields(array(
      'title' => $player['title'],
      'id' => $player['id'],
      'pid' => $player['pid'],
      'guid' => $player['guid'],
      'description' => $player['description'],
      'fid' => $fid,
      'head_html' => media_theplatform_mpx_get_player_html($player['pid'], 'head'),
      'body_html' => media_theplatform_mpx_get_player_html($player['pid'], 'body'),
      'created' => $timestamp,
      'updated' => $timestamp,
      'status' => 1,
    ))
    ->execute();

  // Update file_managed filename with title of player.
  $file_title = db_update('file_managed')
    ->fields(array(
      'filename' => $player['title'],
    ))
    ->condition('fid', $fid, '=')
    ->execute();

  // Write mpx_log record.
  global $user;
  $log = array(
    'uid' => $user->uid,
    'type' => 'player',
    'type_id' => $player_id,
    'action' => 'insert',
    'details' => $details,
  );
  media_theplatform_mpx_insert_log($log);

  // Return code to be used by media_theplatform_mpx_import_all_players().
  return 'insert';
}

/**
 * Updates given Player and File in Media Library.
 *
 * @param array $player
 *   Record of Player data requested from thePlatform
 * @param int $fid
 *   File fid of Player's File in file_managed
 *
 * @return String
 *   Returns 'update' for counters in media_theplatform_mpx_import_all_players()
 */
function media_theplatform_mpx_update_player($player, $fid) {
  $timestamp = REQUEST_TIME;

  // Fetch player_id and status from mpx_player for given $player.
  $mpx_player = db_select('mpx_player', 'p')
    ->fields('p', array('player_id', 'status'))
    ->condition('id', $player['id'], '=')
    ->execute()
    ->fetchAssoc();

  // If we're performing an update, it means this player is active.
  // Check if the player was inactive and is being re-activated:
  if ($mpx_player['status'] == 0) {
    $details = 'player re-activated';
  }
  else {
    $details = NULL;
  }

  // Update mpx_player record.
  $update = db_update('mpx_player')
    ->fields(array(
      'title' => $player['title'],
      'pid' => $player['pid'],
      'guid' => $player['guid'],
      'description' => $player['description'],
      'head_html' => media_theplatform_mpx_get_player_html($player['pid'], 'head'),
      'body_html' => media_theplatform_mpx_get_player_html($player['pid'], 'body'),
      'status' => 1,
      'updated' => $timestamp,
    ))
    ->condition('id', $player['id'], '=')
    ->execute();

  // Update file_managed filename with title of player.
  $file_title = db_update('file_managed')
    ->fields(array(
      'filename' => $player['title'],
    ))
    ->condition('fid', $fid, '=')
    ->execute();

  // Write mpx_log record.
  global $user;
  $log = array(
    'uid' => $user->uid,
    'type' => 'player',
    'type_id' => $mpx_player['player_id'],
    'action' => 'update',
    'details' => $details,
  );
  media_theplatform_mpx_insert_log($log);

  // Return code to be used by media_theplatform_mpx_import_all_players().
  return 'update';
}

/**
 * Returns associative array of mpx_player data for given File $fid.
 */
function media_theplatform_mpx_get_mpx_player_by_fid($fid) {
  return db_select('mpx_player', 'p')
    ->fields('p')
    ->condition('fid', $fid, '=')
    ->execute()
    ->fetchAssoc();
}

/**
 * Returns total number of records in mpx_player table.
 */
function media_theplatform_mpx_get_mpx_player_count() {
  return db_query("SELECT count(player_id) FROM {mpx_player}")->fetchField();
}

/**
 * Returns array of all records in mpx_player table as Objects.
 */
function media_theplatform_mpx_get_all_mpx_players() {
  return db_select('mpx_player', 'p')
    ->fields('p')
    ->execute()
    ->fetchAll();
}

/**
 * Returns CSS extracted from given Head HTML of a Player.
 *
 * @param string $head
 *   HTML from the Player's <HEAD>
 *
 * @return String
 *   Returns all CSS (does not include <style> tags)
 */
function media_theplatform_mpx_get_mpx_player_css($head) {
  // Get inline CSS from all <style> tags (there can be multiple).
  $inline_css = implode(media_theplatform_mpx_extract_all_css_inline($head), "\n");
  $external_css = '';
  // Get css from all external stylesheets.
  $paths = media_theplatform_mpx_extract_all_css_links($head);
  foreach ($paths as $file) {
    $external_css .= media_theplatform_mpx_get_external_css($file) . "\n";
  }
  return $inline_css . "\n" . $external_css;
}

/**
 * Returns JS HTML extracted from given Head HTML of a Player.
 *
 * @param string $head
 *   HTML from the Player's <HEAD>
 *
 * @return String
 *   Returns inline JS in <script></script> tags if the Player has inline JS
 *   Also external links to JS if the Player has external JS
 */
function media_theplatform_mpx_get_mpx_player_js($head) {
  $output = '';
  $inline = media_theplatform_mpx_extract_all_js_inline($head);
  if ($inline) {
    foreach ($inline as $script) {
      $output .= '<script type="text/javascript">' . $script . '</script>' . "\n";
    }
  }
  $js_files = media_theplatform_mpx_extract_all_js_links($head);
  if ($js_files) {
    foreach ($js_files as $src) {
      $output .= '<script type="text/javascript" src="' . $src . '"></script>' . "\n";
    }
  }
  return $output;
}

/**
 * Returns HTML to be used when rendering a mpx Media File.
 *
 * @param string $mpx_id
 *   Unique indentifier for the div id of the mpx Media object.
 * @param string $css
 *   Inline CSS to be placed within <style> tags.
 * @param string $body
 *   HTML for the actual Player.
 *
 * @return String
 *   Renders all components to be placed within a media-mpx-wrapper
 */
function media_theplatform_mpx_render_mpx_player($mpx_id, $css, $body) {
  // Drupal outputs CDATA into the CSS.
  // See http://drupal.org/node/672954#comment-3460304.
  $output = '';
  $output .= '<style type="text/css">' . $css . '</style>';
  $output .= '<div id="mpx-' . $mpx_id . '">' . $body . '</div>';
  return $output;
}
