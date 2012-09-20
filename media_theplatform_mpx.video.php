<?php
/**
 * @file
 * functions for Videos.
 */

/**
 * Returns array of all mpx Feeds in specified thePlatform account.
 */
function media_theplatform_mpx_get_feeds_select() {
  // Check for the signIn token.
  $mpx_token = media_theplatform_mpx_variable_get('token');
  $mpx_account = media_theplatform_mpx_variable_get('import_account');
  if (!$mpx_token || !$mpx_account) {
    return FALSE;
  }

  global $user;
  // Next get the list of feeds from the site.
  // Use 'byDisabled=false' to only grab Feeds that are enabled.
  $feed_url = 'http://data.feed.theplatform.com/feed/data/FeedConfig?schema=2.0.0&form=json&pretty=true&fields=pid,title,plfeed$availableFields&byDisabled=false&token=' . $mpx_token . '&account=' . $mpx_account;
  $result = drupal_http_request($feed_url);
  $result_data = drupal_json_decode($result->data);
  if ($result_data['entryCount'] == 0) {
    $log = array(
      'uid' => $user->uid,
      'type' => 'request',
      'type_id' => NULL,
      'action' => 'feed',
      'details' => '0 feeds returned.',
    );
    media_theplatform_mpx_insert_log($log);
    return FALSE;
  }

  foreach ($result_data['entries'] as $entry) {
    $fields = $entry['plfeed$availableFields'];
    // Feed must contain "title" AND a thumbnail field in availableFields.
    if (in_array('title', $fields) && (in_array('defaultThumbnailUrl', $fields) || in_array('thumbnails.media:', $fields))) {
      $feeds[$entry['plfeed$pid']] = $entry['title'];
    }
  }

  $log = array(
    'uid' => $user->uid,
    'type' => 'request',
    'type_id' => NULL,
    'action' => 'feed',
    'details' => count($feeds) . ' feeds returned.',
  );
  media_theplatform_mpx_insert_log($log);
  return $feeds;
}


/**
 * Returns array of all mpx Media (Video) data from the Import Feed.
 */
function media_theplatform_mpx_get_feed_videos() {
  $result = drupal_http_request(media_theplatform_mpx_get_import_feed_url());
  $result_data = drupal_json_decode($result->data);
  $videos = array();
  global $user;
  foreach ($result_data['entries'] as $video) {
    // If defaultThumbnailUrl is available, use this value.
    if (array_key_exists('defaultThumbnailUrl', $video)) {
      $thumbnail_url = $video['defaultThumbnailUrl'];
    }
    // Else, parse through the thumbnails array.
    else {
      // If no thumbnails exist in thumbnails array, return error.
      if (count($video['media$thumbnails']) == 0) {
        $log = array(
          'uid' => $user->uid,
          'type' => 'request',
          'type_id' => NULL,
          'action' => 'video',
          'details' => 'Error: no thumbnails in feed.',
        );
        media_theplatform_mpx_insert_log($log);
        return "no thumbnails";
      }
      $thumbnail_url = media_theplatform_mpx_parse_thumbnail_url($video['media$thumbnails']);
      //$file_url = media_theplatform_mpx_parse_file_url($video['media$content']);
    }
    $videos[] = array(
      'guid' => $video['guid'],
      'title' => $video['title'],
      'description' => $video['description'],
      'thumbnail_url' => $thumbnail_url,
    );
  }
  $log = array(
    'uid' => $user->uid,
    'type' => 'request',
    'type_id' => NULL,
    'action' => 'video',
    'details' => count($videos) . ' videos returned.',
  );
  media_theplatform_mpx_insert_log($log);
  return $videos;
}

/**
 * Returns URL string of the thumbnail object where isDefault == 1.
 */
function media_theplatform_mpx_parse_thumbnail_url($data) {
  foreach ($data as $record) {
    if ($record['plfile$isDefault']) {
      return $record['plfile$url'];
    }
  }
}

/**
 * Returns thumbnail URL string for given guid from mpx_video table.
 */
function media_theplatform_mpx_get_thumbnail_url($guid) {
  return db_query("SELECT thumbnail_url FROM {mpx_video} WHERE guid=:guid", array(':guid' => $guid))->fetchField();
}

/**
 * Imports all Videos into Media Library.
 *
 * @param String $type
 *   Import type. Possible values 'cron' or 'manual', for sync.
 *
 * @return Array
 *   $data['total'] - # of videos retrieved
 *   $data['num_inserts'] - # of videos added to mpx_video table
 *   $data['num_updates'] - # of videos updated
 *   $data['num_inactives'] - # of videos changed from active to inactive
 */
function media_theplatform_mpx_import_all_videos($type) {

  // Clicked on Videos Sync form.
  if ($type == 'manual') {
    global $user;
    $uid = $user->uid;
  }
  else {
    $uid = 0;
  }
  $log = array(
    'uid' => $uid,
    'type' => 'video',
    'type_id' => NULL,
    'action' => 'import',
    'details' => $type,
  );
  media_theplatform_mpx_insert_log($log);

  // Retrieve list of videos.
  $videos = media_theplatform_mpx_get_feed_videos();
  // If no result, return FALSE.
  if (!$videos) {
    return FALSE;
  }
  // If no thumbnails, return error code.
  elseif ($videos == 'no thumbnails') {
    return $videos;
  }

  // Initalize our counters.
  $num_inserts = 0;
  $num_updates = 0;
  $num_inactives = 0;
  $incoming = array();

  // Loop through videos retrieved.
  foreach ($videos as $video) {
    // Keep track of the incoming guid.
    $incoming[] = $video['guid'];
    // Import this video.
    $op = media_theplatform_mpx_import_video($video);
    if ($op == 'insert') {
      $num_inserts++;
    }
    elseif ($op == 'update') {
      $num_updates++;
    }
  }

  $num_inactives = 0;

  // Find all mpx_videos NOT in $incoming with status = 1.
  $inactives = db_select('mpx_video', 'v')
    ->fields('v', array('video_id', 'fid'))
    ->condition('guid', $incoming, 'NOT IN')
    ->condition('status', 1, '=')
    ->execute();

  global $user;

  // Loop through results:
  while ($record = $inactives->fetchAssoc()) {
    // Set status to inactive.
    $inactive = db_update('mpx_video')
      ->fields(array('status' => 0))
      ->condition('video_id', $record['video_id'], '=')
      ->execute();

    // Write mpx_log record.
    $log = array(
      'uid' => $user->uid,
      'type' => 'video',
      'type_id' => $record['video_id'],
      'action' => 'inactive',
      'details' => NULL,
    );
    media_theplatform_mpx_insert_log($log);
    $num_inactives++;
  }

  // Return counters as an array.
  return array(
    'total' => count($videos),
    'inserts' => $num_inserts,
    'updates' => $num_updates,
    'inactives' => $num_inactives,
  );
}

/**
 * Updates or inserts given Video within Media Library.
 *
 * @param array $video
 *   Record of Video data requested from thePlatform Import Feed
 *
 * @return String
 *   Returns output of media_theplatform_mpx_update_video() or media_theplatform_mpx_insert_video()
 */
function media_theplatform_mpx_import_video($video) {
  // Check if fid exists in files table for URI = mpx://m/GUID/*.
  $guid = $video['guid'];
  $files = media_theplatform_mpx_get_files_by_guid($guid);
  
  /*
  $player_id = media_theplatform_mpx_variable_get('default_player_fid');
  $uri = 'mpx://m/' . $guid . '/p/' . $player_id;
  $fid = db_query("SELECT fid FROM {file_managed} WHERE uri=:uri", array(':uri' => $uri))->fetchField();
  */

  // If a file exists:
  if ($files) {
    // Check if record already exists in mpx_video.
    $imported = db_query("SELECT video_id FROM {mpx_video} WHERE guid=:guid", array(':guid' => $guid))->fetchField();
    // If mpx_video record exists, then update record.
    if ($imported) {
      return media_theplatform_mpx_update_video($video);
    }
    // Else insert new mpx_video record with existing $fid.
    else {
      return media_theplatform_mpx_insert_video($video, $files[0]->fid);
    }
  }
  // Else insert new file/record:
  else {
    return media_theplatform_mpx_insert_video($video, NULL);
  }
}

/**
 * Inserts given Video and File into Media Library.
 *
 * @param array $video
 *   Record of Video data requested from thePlatform Import Feed
 * @param int $fid
 *   File fid of Video's File in file_managed if it already exists
 *   NULL if it doesn't exist
 *
 * @return String
 *   Returns 'insert' for counters in media_theplatform_mpx_import_all_videos()
 */
function media_theplatform_mpx_insert_video($video, $fid = NULL) {
  $timestamp = REQUEST_TIME;
  $player_id = media_theplatform_mpx_variable_get('default_player_fid');
  
  // If file doesn't exist, write it to file_managed.
  if (!$fid) {
    // Build embed string to create file:
    // "m" is for media.
    $embed_code = 'mpx://m/' . $video['guid'] . '/p/' . $player_id;
    // Create the file.
    $provider = media_internet_get_provider($embed_code);
    $file = $provider->save();
    $fid = $file->fid;
    $details = 'new fid = ' . $fid;
  }
  else {
    $details = 'existing fid = ' . $fid;
  }
  
  // Insert record into mpx_video.
  $video_id = db_insert('mpx_video')
    ->fields(array(
      'title' => $video['title'],
      'guid' => $video['guid'],
      'description' => $video['description'],
      'fid' => $fid,
      'account' => media_theplatform_mpx_variable_get('import_account'),
      'thumbnail_url' => $video['thumbnail_url'],
      'created' => $timestamp,
      'updated' => $timestamp,
      'status' => 1,
    ))
    ->execute();
  // Load default Player for appending to Filename
  $player = media_theplatform_mpx_get_mpx_player_by_fid($player_id);
  media_theplatform_mpx_update_video_filename($fid, $video['title'], $player['title']);

  // Write mpx_log record.
  global $user;
  $log = array(
    'uid' => $user->uid,
    'type' => 'video',
    'type_id' => $video_id,
    'action' => 'insert',
    'details' => $details,
  );
  media_theplatform_mpx_insert_log($log);

  // Return code to be used by media_theplatform_mpx_import_all_videos().
  return 'insert';
}

/**
 * Updates File $fid with given $video_title and $player_title.
 */
function media_theplatform_mpx_update_video_filename($fid, $video_title, $player_title) {
  // Update file_managed filename with title of video.
  $file_title = db_update('file_managed')
    ->fields(array(
      'filename' => $video_title . ' - ' . $player_title,
    ))
    ->condition('fid', $fid, '=')
    ->execute();  
}

/**
 * Updates given Video and File in Media Library.
 *
 * @param array $video
 *   Record of Video data requested from thePlatform Import Feed
 *
 * @return String
 *   Returns 'update' for counters in media_theplatform_mpx_import_all_players()
 */
function media_theplatform_mpx_update_video($video) {
  $timestamp = REQUEST_TIME;

  // Fetch video_id and status from mpx_video table for given $video.
  $mpx_video = media_theplatform_mpx_get_mpx_video_by_field('guid', $video['guid']);

  // If we're performing an update, it means this video is active.
  // Check if the video was inactive and is being re-activated:
  if ($mpx_video['status'] == 0) {
    $details = 'video re-activated';
  }
  else {
    $details = NULL;
  }

  // Update mpx_video record.
  $update = db_update('mpx_video')
    ->fields(array(
      'title' => $video['title'],
      'guid' => $video['guid'],
      'description' => $video['description'],
      'thumbnail_url' => $video['thumbnail_url'],
      'status' => 1,
      'updated' => $timestamp,
    ))
    ->condition('guid', $video['guid'], '=')
    ->execute();

  // @todo: (maybe). Update all files with guid with new title if the title is different.

  $image_path = 'media-mpx/' . $video['guid'] . '.jpg';
  // Delete thumbnail from files/media-mpx directory.
  file_unmanaged_delete('public://' . $image_path);
  // Delete thumbnail from all the styles.
  // Now, the next time file is loaded, MediaThePlatformMpxStreamWrapper
  // will call getOriginalThumbnail to update image.
  image_path_flush($image_path);
  // Write mpx_log record.
  global $user;
  $log = array(
    'uid' => $user->uid,
    'type' => 'video',
    'type_id' => $mpx_video['video_id'],
    'action' => 'update',
    'details' => $details,
  );
  media_theplatform_mpx_insert_log($log);

  // Return code to be used by media_theplatform_mpx_import_all_videos().
  return 'update';
}

/**
 * Returns associative array of mpx_video data for given File $fid.
 */
function media_theplatform_mpx_get_mpx_video_by_fid($fid) {
  return db_select('mpx_video', 'v')
    ->fields('v')
    ->condition('fid', $fid, '=')
    ->execute()
    ->fetchAssoc();
}

/**
 * Returns associative array of mpx_video data for given $field and its $value.
 */
function media_theplatform_mpx_get_mpx_video_by_field($field, $value) {
  return db_select('mpx_video', 'v')
    ->fields('v')
    ->condition($field, $value, '=')
    ->execute()
    ->fetchAssoc();
}

/**
 * Returns total number of records in mpx_video table.
 */
function media_theplatform_mpx_get_mpx_video_count() {
  return db_query("SELECT count(video_id) FROM {mpx_video}")->fetchField();
}


/**
 * Returns array of all records in mpx_video table as Objects.
 */
function media_theplatform_mpx_get_all_mpx_videos() {
  return db_select('mpx_video', 'v')
    ->fields('v')
    ->execute()
    ->fetchAll();
}

/**
 * Returns array of all records in file_managed with mpx://m/$guid/%
 */
function media_theplatform_mpx_get_files_by_guid($guid) {
  return db_select('file_managed', 'f')
    ->fields('f')
    ->condition('uri', 'mpx://m/' . $guid . '/%', 'LIKE')
    ->execute()
    ->fetchAll();
}

/**
 * Returns array of all records in file_managed with mpx://m/%/p/[player_fid]
 */
function media_theplatform_mpx_get_files_by_player_fid($fid) {
  return db_select('file_managed', 'f')
    ->fields('f')
    ->condition('uri', 'mpx://m/%', 'LIKE')
    ->condition('uri', '%/p/' . $fid, 'LIKE')
    ->execute()
    ->fetchAll();
}
