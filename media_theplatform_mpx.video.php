<?php
/**
 * @file
 * functions for Videos.
 */

/**
 * Query thePlatform Media Notify service to get Media id's that have changed.
 *
 * @param String $since
 *   The last notfication ID from thePlatform used to Sync mpxMedia.
 *
 * @return Array
 *   Array of mpxMedia id's that have changed since $since.
 */
function media_theplatform_mpx_get_changed_ids($since) {
  $url = 'http://data.media.theplatform.com/media/notify?token=' . media_theplatform_mpx_variable_get('token') . '&account=' . media_theplatform_mpx_variable_get('import_account') . '&block=false&filter=Media&clientId=drupal_media_theplatform_mpx_' . media_theplatform_mpx_variable_get('account_pid') . '&since=' . $since;
  $result = drupal_http_request($url);
  if (isset($result->data)) {
    $result_data = drupal_json_decode($result->data);
    if (isset($result_data) && count($result_data) > 0) {
      // Initialize arrays to store active and deleted id's.
      $actives = array();
      $deletes = array();

      foreach ($result_data as $changed) {
        // If no method has been returned, there are no changes.
        if (!isset($changed['method'])) {
          return false;
        }
        // Store last notification.
        $last_notification = $changed['id'];
        // Grab the last component of the URL.
        $media_id = basename($changed['entry']['id']);
        if ($changed['method'] !== 'delete') {
          // If this entry id isn't already in actives, add it.
          if (!in_array($media_id, $actives)) {
            $actives[] = $media_id;
          }
        }
        else {
          // Only add to deletes array if this mpxMedia already exists.
          $video_exists = media_theplatform_mpx_get_mpx_video_by_field('id', $media_id);
          if ($video_exists) {
            $deletes[] = $media_id;
          }
        }
      }

      // Remove any deletes from actives, because it causes errors when updating.
      $actives = array_diff($actives, $deletes);
      return array(
        'actives' => $actives,
        'deletes' => $deletes,
        'last_notification' => $last_notification,
      );
    }
  }
  return false;
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
 * @return String
 *   Returns 'insert' for counters in media_theplatform_mpx_import_all_videos()
 */
function media_theplatform_mpx_insert_video($video, $fid = NULL) {
  $timestamp = REQUEST_TIME;
  $player_id = !empty($video['player_id']) ? $video['player_id'] : media_theplatform_mpx_variable_get('default_player_fid');

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
      'player_id' => !empty($video['player_id']) ? $video['player_id'] : null,
      'account' => media_theplatform_mpx_variable_get('import_account'),
      'thumbnail_url' => $video['thumbnail_url'],
      'release_id' => !empty($video['release_id']) ? $video['release_id'] : '',
      'created' => $timestamp,
      'updated' => $timestamp,
      'status' => 1,
      'id' => $video['id'],
    ))
    ->execute();

  // Load default mpxPlayer for appending to Filename.
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

  // If fields are mapped, save them to the file entity
  media_theplatform_mpx_update_file_fields($fid, $video['fields']);

  // Return code to be used by media_theplatform_mpx_import_all_videos().
  return 'insert';
}

/**
 * Updates field values on a given file entity. Fields array must
 * be indexed by the field names to be updated.
 * @param $fid
 * @param $fields
 */
function media_theplatform_mpx_update_file_fields($fid, $fields) {
  if(count($fields)) {
    $wrapper = entity_metadata_wrapper('file', $fid);
    $properties = $wrapper->getPropertyInfo();
    foreach($fields as $field_name => $field_value) {
      // Make sure the field actually exists on the file entity
      if(isset($properties[$field_name])) {
        try {
          $fi = field_info_field($field_name);
          if($fi['type'] == 'taxonomy_term_reference' && !is_array($field_value))
            $field_value = array($field_value);
          if(is_array($field_value) || strlen($field_value))
            $wrapper->{$field_name}->set($field_value);
        } catch (EntityMetadataWrapperException $e) {
          $message = t('Caught an exception while trying to sync field @field_name: @exception', array('@field_name' => $field_name, '@exception' => $e->getMessage()));
          watchdog('media_theplatform_mpx', $message, array(), WATCHDOG_ERROR);
          drupal_set_message($message, 'error');
        }
      }
    }
    $wrapper->save(true);
  }
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
      'release_id' => $video['release_id'],
      'status' => 1,
      'updated' => $timestamp,
      'id' => $video['id'],
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

  // If fields are mapped, save them to the file entity
  media_theplatform_mpx_update_file_fields($mpx_video['fid'], $video['fields']);

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
 * Returns array of all records in file_managed with mpx://m/$guid/%.
 */
function media_theplatform_mpx_get_files_by_guid($guid) {
  return db_select('file_managed', 'f')
    ->fields('f')
    ->condition('uri', 'mpx://m/' . $guid . '/%', 'LIKE')
    ->execute()
    ->fetchAll();
}

/**
 * Returns array of all records in file_managed with mpx://m/%/p/[player_fid].
 */
function media_theplatform_mpx_get_files_by_player_fid($fid) {
  return db_select('file_managed', 'f')
    ->fields('f')
    ->condition('uri', 'mpx://m/%', 'LIKE')
    ->condition('uri', '%/p/' . $fid, 'LIKE')
    ->execute()
    ->fetchAll();
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
 * Returns most recent notification sequence number from thePlatform.
 */
function media_theplatform_mpx_set_last_notification() {
  $url = 'http://data.media.theplatform.com/media/notify?token=' . media_theplatform_mpx_variable_get('token') . '&account=' . media_theplatform_mpx_variable_get('import_account') . '&filter=Media&clientId=drupal_media_theplatform_mpx_' . media_theplatform_mpx_variable_get('account_pid');
  $result = drupal_http_request($url);
  $result_data = drupal_json_decode($result->data);
  if ($result_data) {
    media_theplatform_mpx_variable_set('last_notification', $result_data[0]['id']);
  }
  return false;
}

/**
 * Query thePlatform Media service to get all published mpxMedia files.
 *
 * @param String $id
 *   Value for mpx_video.id.
 *
 * @param String $op
 *   Valid values: 'unpublished' or 'deleted'.
 */
function media_theplatform_mpx_set_mpx_video_inactive($id, $op) {
  // Set status to inactive.
  $inactive = db_update('mpx_video')
  ->fields(array('status' => 0))
  ->condition('id', $id, '=')
  ->execute();

  // Let other modules perform operations when videos are disabled.
  module_invoke_all('media_theplatform_mpx_set_video_inactive', $id, $op);

  // Write mpx_log record.
  // @todo: Set type_id to $id once type_id gets changed to varchar.
  global $user;
  $log = array(
    'uid' => $user->uid,
    'type' => 'video',
    'type_id' => NULL,
    'action' => $op,
    'details' => $id,
  );
  media_theplatform_mpx_insert_log($log);
}

/**
 * Video queue item worker callback.
 */
function process_media_theplatform_mpx_video_cron_queue_item($item) {
  switch ($item['queue_operation']) {
    case 'publish':
      // Import/Update the video.
      if (!empty($item['video'])) {
        $file_field_map = media_theplatform_mpx_variable_get('file_field_map', false);
        $video = $item['video'];
        $fields = array();
        if($file_field_map)
          $fields = _mpx_fields_extract_mpx_field_values($video, unserialize($file_field_map));
        // Add item to video queue.
        $video_item = array(
          'id' => basename($video['id']),
          'guid' => $video['guid'],
          'title' => $video['title'],
          'description' => $video['description'],
          'thumbnail_url' => $video['plmedia$defaultThumbnailUrl'],
          'release_id' => $video['media$content'][0]['plfile$releases'][0]['plrelease$pid'],
          'fields' => $fields,
        );
        // Allow modules to alter the video item for pulling in custom metadata.
        drupal_alter('media_theplatform_mpx_media_import_item', $video_item, $video);
        // Perform the import/update.
        media_theplatform_mpx_import_video($video_item);
      }
      break;
    case 'unpublish':
      if (!empty($item['unpublish_id'])) {
        media_theplatform_mpx_set_mpx_video_inactive($item['unpublish_id'], 'unpublished');
      }
      break;
    case 'delete':
      if (!empty($item['delete_id'])) {
        media_theplatform_mpx_set_mpx_video_inactive($item['delete_id'], 'deleted');
      }
      break;
  }
}

/**
 * Helper that retrieves and returns data from an MPX feed url.
 */
function _media_theplatform_mpx_retrieve_feed_data($url) {
  // Inform site admins we're attempting to retrieve data.
  drupal_set_message(t('Attempting to retrieve data from the following feed url: @url',
    array('@url' => $url)));
  watchdog('media_theplatform_mpx', 'Attempting to retrieve data from the following feed url: @url',
    array('@url' => $url));

  // Fetch the actual feed data.
  $feed_request_timeout = media_theplatform_mpx_variable_get('cron_videos_timeout', 120);
  $result = drupal_http_request($url, array('timeout' => $feed_request_timeout));

  if (!isset($result->data)) {
    drupal_set_message(t('Video import/update failed.  Could not retrieve data from the following feed request: @url',
      array('@url' => $url)));
    watchdog('media_theplatform_mpx', 'Video import/update failed.  Could not retrieve data from the following feed request: @url',
      array('@url' => $url), WATCHDOG_ERROR);
    return false;
  }

  $result_data = drupal_json_decode($result->data);

  if (!$result_data) {
    drupal_set_message(t('Video import/update failed.  Could not parse JSON data from the following feed request: @url',
      array('@url' => $url)));
    watchdog('media_theplatform_mpx', 'Video import/update failed.  Could not parse JSON data from the following feed request: @url',
      array('@url' => $url), WATCHDOG_ERROR);

    return false;
  }

  return $result_data;
}

/**
 * Helper that retrieves and returns data from an MPX feed url.
 */
function _media_theplatform_mpx_process_video_import_feed_data($result_data, $media_to_update = NULL) {
  // Initalize arrays to store mpxMedia data.
  $queue_items = array();
  $num_inserted_or_updated = 0;
  $num_unpublished = 0;
  $num_deleted = 0;
  $published_ids = array();
  // If we have any inserts/updates, $result_data will be filled
  // If it is null, we're only performing unpublish/deletes
  if(isset($result_data)) {
    // Keys entryCount and entries are only set when there is more than 1 entry.
    if (isset($result_data['entryCount'])) {
      foreach ($result_data['entries'] as $video) {
        $published_ids[] = basename($video['id']);

        // Add item to queue
        $item = array(
          'queue_operation' => 'publish',
          'video' => $video,
        );
        $queue_items[] = $item;
        $num_inserted_or_updated++;
      }
    }
    // If only one row, result_data holds all the mpxMedia data (if its published).
    // If responseCode is returned, it means 1 id in $ids has been unpublished and would return no data.
    else if (!isset($result_data['responseCode'])) {
      $video = $result_data;
      $published_ids[] = basename($video['id']);
      // Add item to video queue.
      $item = array(
        'queue_operation' => 'publish',
        'video' => $video,
      );
      $queue_items[] = $item;
      $num_inserted_or_updated++;
    }
  }

  // Store any ids that have been unpublished.
  if (isset($media_to_update)) {
    $unpublished_ids = array_diff($media_to_update['actives'], $published_ids);
    // Filter out any ids which haven't been imported.
    foreach ($unpublished_ids as $key => $unpublished_id) {
      $video_exists = media_theplatform_mpx_get_mpx_video_by_field('id', $unpublished_id);
      if (!$video_exists) {
        unset($unpublished_ids[$key]);
      }
      else {
        // Add to queue.
        $item = array(
          'queue_operation' => 'unpublish',
          'unpublish_id' => $unpublished_id,
        );
        $queue_items[] = $item;
        $num_unpublished++;
      }
    }
  }

  // Collect any deleted mpxMedia.
  if ($media_to_update && count($media_to_update['deletes'] > 0)) {
    foreach ($media_to_update['deletes'] as $delete_id) {
      // Add to queue.
      $item = array(
        'queue_operation' => 'delete',
        'delete_id' => $delete_id,
      );
      $queue_items[] = $item;
      $num_deleted++;
    }
  }

  // Add the queue items to the cron queueu.
  $queue = DrupalQueue::get('media_theplatform_mpx_video_cron_queue');
  foreach ($queue_items as $queue_item) {
    $queue->createItem($queue_item);
  }

  // Notify site admin of number of items processed.
  drupal_set_message(t(':num mpxMedia queued to be created or updated.', array(':num' => $num_inserted_or_updated)));
  drupal_set_message(t(':num existing mpxMedia queued to be unpublished.', array(':num' => $num_unpublished)));
  drupal_set_message(t(':num existing mpxMedia queued to be deleted.', array(':num' => $num_deleted)));

  return true;
}

/**
 * Processes a batch import/update.
 */
function _media_theplatform_mpx_process_batch_video_import($type) {
  // Log what type of operation we're performing.
  global $user;
  $log = array(
      'uid' => $type == 'manual' ? $user->uid : 0,
      'type' => 'video',
      'type_id' => NULL,
      'action' => 'batch',
      'details' => $type,
    );
  media_theplatform_mpx_insert_log($log);

  // Get the parts for the batch url and construct it.
  $batch_url = media_theplatform_mpx_variable_get('proprocessing_batch_url', '');
  $batch_item_count = media_theplatform_mpx_variable_get('proprocessing_batch_item_count', 0);
  $current_batch_item = media_theplatform_mpx_variable_get('proprocessing_batch_current_item', 1);
  $feed_request_item_limit = media_theplatform_mpx_variable_get('cron_videos_per_run', 500);

  $url = $batch_url . '&range=' . $current_batch_item . '-' . ($current_batch_item + ($feed_request_item_limit-1));

  $result_data = _media_theplatform_mpx_retrieve_feed_data($url);
  if (!$result_data) {
    return false;
  }

  $processesing_success = _media_theplatform_mpx_process_video_import_feed_data($result_data);
  if (!$processesing_success) {
    return false;
  }

  $current_batch_item += $feed_request_item_limit;
  if ($current_batch_item < $batch_item_count) {
    media_theplatform_mpx_variable_set('proprocessing_batch_current_item', $current_batch_item);
  }
  else {
    // Reset the batch system variables.
    media_theplatform_mpx_variable_set('proprocessing_batch_url', '');
    media_theplatform_mpx_variable_set('proprocessing_batch_item_count', '');
    media_theplatform_mpx_variable_set('proprocessing_batch_current_item', '');
    // In case this is the end of the initial import batch, set the last
    // notification id.
    $last_update_notification = media_theplatform_mpx_variable_get('last_notification');
    if (!$last_update_notification) {
      media_theplatform_mpx_set_last_notification();
    }
  }
}

/**
 * Helper that constructs a video feed url given a comma-delited list of video
 * ids or "all" for all media (used during initial import).
 */
function _media_theplatform_mpx_get_video_feed_url($ids = NULL) {
  $ids = ($ids == 'all' || !$ids) ? '' : '/' . $ids;
  $token = media_theplatform_mpx_variable_get('token');
  $account = urlencode(media_theplatform_mpx_variable_get('account_id'));

  // Note:  We sort by updated date ascending so that newly updated content is
  // fetched from the feed last.  In cases of large batches, e.g. an initial
  // import, media can be edited before the batch is complete.  This ensures
  // these edited media will be fetched toward the end of the batch.
  $url = 'http://data.media.theplatform.com/media/data/Media' . $ids .
        '?schema=1.4.0&form=json&pretty=true&byOwnerId=' . $account .
        '&byContent=byHasReleases%3Dtrue&sort=guid|asc&token=' . $token;

  return $url;
}

/**
 * Get the total item count for a given feed url.
 */
function _media_theplatform_mpx_get_feed_item_count($url) {
  $count_url = $url . '&count=true&fields=guid&range=1-1';
  $feed_request_timeout = media_theplatform_mpx_variable_get('cron_videos_per_run', 120);

  $count_result = drupal_http_request($count_url, array('timeout' => $feed_request_timeout));

  if (empty($count_result->data)) {
      return false;
   }

  $count_result_data = drupal_json_decode($count_result->data);
  $total_result_count = isset($count_result_data['totalResults']) ?
      $count_result_data['totalResults'] : NULL;

  if (!$total_result_count) {
    drupal_set_message(t('Video import/update failed.  Could not retrieve the total result count for the following feed request: @url',
      array('@url' => $count_url)));
    watchdog('media_theplatform_mpx', 'Video import/update failed.  Could not retrieve the total result count for the following feed request: @url',
      array('@url' => $count_url), WATCHDOG_ERROR);
    return false;
  }

  return $total_result_count;
}


/**
 * Processes a video update.
 */
function _media_theplatform_mpx_process_video_update($since, $type) {
  // Log what type of operation we're performing.
  global $user;
  $log = array(
    'uid' => $type == 'manual' ? $user->uid : 0,
    'type' => 'video',
    'type_id' => NULL,
    'action' => 'update',
    'details' => $type,
  );
  media_theplatform_mpx_insert_log($log);

  // Get all IDs of mpxMedia that have been updated since last notification.
  $media_to_update = media_theplatform_mpx_get_changed_ids($since);

  if (!$media_to_update) {
    $cron_queue = DrupalQueue::get('media_theplatform_mpx_video_cron_queue');
    $num_cron_queue_items = $cron_queue->numberOfItems();
    if ($num_cron_queue_items) {
      $message = t('All mpxMedia is up to date.  There are approximately @num_items videos waiting to be processed.',
      array('@num_items' => $num_cron_queue_items));
    }
    else {
      $message = t('All mpxMedia is up to date.');
    }
    drupal_set_message($message);
    return false;
  }

  // If we have actives, join their ids together for the data url
  if (count($media_to_update['actives']) > 0) {
    $ids = implode(',', $media_to_update['actives']);
  }
  // Otherwise, just handle the deletes/unpublishes now
  else {
    $processesing_success = _media_theplatform_mpx_process_video_import_feed_data(null, $media_to_update);
    if (!$processesing_success) {
      return false;
    } else {
      media_theplatform_mpx_variable_set('last_notification', $media_to_update['last_notification']);
      return true;
    }
  }

  // Get the feed url.
  $url = _media_theplatform_mpx_get_video_feed_url($ids);

  // Get the total result count for this update.  If it is greater than the feed
  // request item limit, start a new batch.
  $total_result_count = count(explode(',', $ids));
  $feed_request_item_limit = media_theplatform_mpx_variable_get('cron_videos_per_run', 500);

  if ($total_result_count && $total_result_count > $feed_request_item_limit) {
    // Set last notification for the next update.
    media_theplatform_mpx_variable_set('last_notification', $media_to_update['last_notification']);
    // Set starter batch system variables.
    media_theplatform_mpx_variable_set('proprocessing_batch_url', $url);
    media_theplatform_mpx_variable_set('proprocessing_batch_item_count', $total_result_count);
    media_theplatform_mpx_variable_set('proprocessing_batch_current_item', 1);
    // Perform the first batch operation, not the update.
    return _media_theplatform_mpx_process_batch_video_import($type);
  }
  $result_data = _media_theplatform_mpx_retrieve_feed_data($url);

  if (!$result_data) {
    return false;
  }
  $processesing_success = _media_theplatform_mpx_process_video_import_feed_data($result_data, $media_to_update);
  if (!$processesing_success) {
    return false;
  }
  // Set last notification for the next update.
  media_theplatform_mpx_variable_set('last_notification', $media_to_update['last_notification']);

  return true;
}

/**
 * Processes a video update.
 */
function _media_theplatform_mpx_process_video_import($type) {
  drupal_set_message('Running initial video import.');

  // Log what type of operation we're performing.
  global $user;
  $log = array(
    'uid' => $type == 'manual' ? $user->uid : 0,
    'type' => 'video',
    'type_id' => NULL,
    'action' => 'import',
    'details' => $type,
  );
  media_theplatform_mpx_insert_log($log);

  // Set the first last notification value for subsequent updates.  Setting it
  // now ensures that any updates that happen during the import are processed
  // in subsequent updates.
  media_theplatform_mpx_set_last_notification();

  // Get the feed url.
  $url = _media_theplatform_mpx_get_video_feed_url();

  // Get the total result count for this update.  If it is greater than the feed
  // request item limit, start a new batch.
  $total_result_count = _media_theplatform_mpx_get_feed_item_count($url);
  $feed_request_item_limit = media_theplatform_mpx_variable_get('cron_videos_per_run', 500);

  if ($total_result_count && $total_result_count > $feed_request_item_limit) {
    // Set starter batch system variables.
    media_theplatform_mpx_variable_set('proprocessing_batch_url', $url);
    media_theplatform_mpx_variable_set('proprocessing_batch_item_count', $total_result_count);
    media_theplatform_mpx_variable_set('proprocessing_batch_current_item', 1);
    // Perform the first batch operation, not the update.
    return _media_theplatform_mpx_process_batch_video_import($type);
   }

  $result_data = _media_theplatform_mpx_retrieve_feed_data($url);
  if (!$result_data) {
    return false;
  }
  $processesing_success = _media_theplatform_mpx_process_video_import_feed_data($result_data);
  if (!$processesing_success) {
    return false;
  }

  return true;
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
  // Check if we're running a feed request batch.  If so, construct the batch url.
  $batch_url = media_theplatform_mpx_variable_get('proprocessing_batch_url', '');
  if ($batch_url) {
    return _media_theplatform_mpx_process_batch_video_import($type);
  }

  // Check if we have a notification stored.  If so, run an update.
  $last_update_notification = media_theplatform_mpx_variable_get('last_notification');
  if ($last_update_notification) {
    return _media_theplatform_mpx_process_video_update($last_update_notification, $type);
  }

  // No last notification set, so this would be an initial import.
  return _media_theplatform_mpx_process_video_import($type);
}
 