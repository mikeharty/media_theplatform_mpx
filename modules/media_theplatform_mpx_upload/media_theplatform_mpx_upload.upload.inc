<?php

/**
 * Fetches a list of publishing profiles for the
 * currently selected MPX account.
 * @return array|bool
 */
function media_theplatform_mpx_upload_get_publishing_profiles() {
  if(media_theplatform_mpx_check_token()) {
    $mpx_token = media_theplatform_mpx_variable_get('token');
    $account_id = media_theplatform_mpx_variable_get('account_id');
    $url = MPX_PD_BASE.'/publish/data/PublishProfile?range=1-100&sort=title&count=true&token='.$mpx_token.'&account='.$account_id.'&schema=1.7.0&fields=description%2Cguid%2Cid%2Ctitle%2Cupdated%2Cadded%2CaddedByUserId%2Clocked%2CownerId%2CupdatedByUserId%2Cversion%2Cplprofile%24disabled%2Cplprofile%24fileTargetCount%2Cplprofile%24outletProfileIds%2Cplprofile%24publishProfileIds&form=json';
    $ch = curl_init($url);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_HEADER, 0);
    $response = curl_exec($ch);
    $responseCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);

    // Check for valid cURL response
    $responseCode = media_theplatform_mpx_check_curl_response_code($url, $responseCode);
    if($responseCode['status'] == 'error') {
      drupal_set_message($responseCode['response'], 'error');
      return false;
    }

    $response = media_theplatform_mpx_check_json_response($response, 'getPublishingProfiles');
    if($response['status'] == 'error') {
      drupal_set_message($response['response'], 'error');
      return false;
    }
    else {
      $profiles = array();
      foreach($response['response']->entries as $profile) {
        $profiles[$profile->id] = $profile->title;
      }

      if(!count($profiles)) {
        drupal_set_message(t('No publishing profiles found for the selected account.'), 'error');
        return false;
      } else {
        return $profiles;
      }
    }
  } else {
    drupal_set_message(t('Unable to sign in to MPX.'), 'error');
    return false;
  }
}

/**
 * Fetches a list of active/unlocked upload servers for
 * the currently selected MPX account.
 * @return array|bool
 */
function media_theplatform_mpx_upload_get_servers() {
  if(media_theplatform_mpx_check_token()) {
    $mpx_token = media_theplatform_mpx_variable_get('token');
    $account_id = media_theplatform_mpx_variable_get('account_id');
    $url = MPX_MD_BASE.'/media/data/Server?sort=title&byOwnerId='.$account_id.'&count=true&token='.$mpx_token.'&schema=1.0&fields=plserver%24disabled%2Clocked%2Ctitle%2Cid&form=json';
    $ch = curl_init($url);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_HEADER, 0);
    $response = curl_exec($ch);
    $responseCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);

    $responseCode = media_theplatform_mpx_check_curl_response_code($url, $responseCode);
    if($responseCode['status'] == 'error') {
      drupal_set_message($responseCode['response'], 'error');
      return false;
    }

    $response = media_theplatform_mpx_check_json_response($response, 'getUploadServer');
    if($response['status'] == 'error') {
      drupal_set_message($response['response'], 'error');
      return false;
    }
    else {
      $servers = array();
      foreach($response['response']->entries as $server) {
        if(!$server->locked && !$server->{'plserver$disabled'})
          $servers[$server->id] = $server->title;
      }

      if(!count($servers)) {
        drupal_set_message(t('No upload servers found for the selected account.'), 'error');
        return false;
      } else {
        return $servers;
      }
    }
  } else {
    drupal_set_message(t('Unable to sign in to MPX.'), 'error');
    return false;
  }
}

/**
 * Inserts an upload into the mpx_uploads table
 * @param $guid
 * @param $media_object_id
 * @param $publish_profiles
 * @param $title
 * @return DatabaseStatementInterface|int|null
 */
function _mpx_uploads_insert($guid, $media_object_id, $publish_profiles, $title) {
  $time = time();
  $mpx_queue_id = db_insert('mpx_uploads')
                    ->fields(array(
                      'guid' => $guid,
                      'media_object_id' => $media_object_id,
                      'title' => $title,
                      'publish_profiles' => serialize($publish_profiles),
                      'created' => $time,
                      'updated' => $time,
                    ))
                    ->execute();

  _mpx_uploads_status_update($mpx_queue_id, 'new');
  return $mpx_queue_id;
}

/**
 * Triggers a non-blocking HTTP request to the publish queue worker
 * @param $mpx_queue_id
 * @return array
 */
function _mpx_publish_worker_trigger($mpx_queue_id) {
  $resp = array('status' => '', 'message' => '');
  $options = array(
    'blocking' => FALSE,
    'headers' => array(
      // Set the Host header to self.
      'Host' => $_SERVER['HTTP_HOST'],
    ),
  );
  $url = httprl_build_url_self('?q=mpx-worker/publish/'.$mpx_queue_id);
  httprl_request($url, $options);
  httprl_send_request();

  $resp['status'] = 'success';
  return $resp;
}


/**
 * Background process for publishing MPX videos
 * @param $mpx_queue_id
 */
function media_theplatform_mpx_publish_worker($mpx_queue_id) {
  // This could take a while...
  set_time_limit(0);

  if(media_theplatform_mpx_check_token()) {
    // Fetch upload item fields
    $upload = db_select('mpx_uploads', 'q')
              ->fields('q')
              ->condition('id', $mpx_queue_id, '=')
              ->execute()
              ->fetchAll();
    $upload = $upload[0];
    $media_object_id = $upload->media_object_id;
    $publish_profiles = unserialize($upload->publish_profiles);
    $publish = _mpx_publish_media($media_object_id, $publish_profiles);
    if($publish['status'] == 'error') {
      drupal_set_message($publish['response'], 'error');
      watchdog('media_theplatform_mpx_upload', $publish['response'], array(), WATCHDOG_ERROR);
      _mpx_uploads_status_update($mpx_queue_id, 'failed', $publish['response']);
    } else {
      // Start the publishing status watcher
      _mpx_uploads_status_update($mpx_queue_id, 'publishing');
      _mpx_publishing_status_watcher();
    }
  } else {
    watchdog('media_theplatform_mpx_upload', 'Unable to sign in to MPX account when attempting to upload via upload queue worker.', array(), WATCHDOG_ERROR);
  }
}

/**
 * Background process which watches the status of uploaded videos,
 * checks the published state of each selected publishing profile
 * and updates videos once all of their publishing actions are complete.
 */
function _mpx_publishing_status_watcher() {
  if(media_theplatform_mpx_check_token()) {
    $mpx_token = media_theplatform_mpx_variable_get('token');
    $account_id = media_theplatform_mpx_variable_get('account_id');
    // Set a semaphore lock for the running state of this thread.
    // This has a very high timeout, because videos can be added to the queue continuously
    // from other threads. There is a safety check, which runs on cron, which will
    // automatically fail any videos which have been publishing for > 6 hours, eventually
    // releasing this thread.
    if(lock_acquire('mpx_publishing_watcher_running', 432000)) {
      while(1) {
        // Fetch all videos marked as "publishing" in the mpx_uploads table.
        // This is checked on each pass, so that videos added by separate threads
        // will be picked up by this (locked) thread.
        $publishing_videos = db_select('mpx_uploads', 'q')
          ->fields('q')
          ->condition('status', 'publishing', '=')
          ->execute()
          ->fetchAll();

        // Call the ProfileResult endpoint for each video
        if(count($publishing_videos)) {
          foreach($publishing_videos as $publishing_video) {
            $media_object_id = $publishing_video->media_object_id;
            $url = MPX_WF_BASE.'/workflow/data/ProfileResult?byMediaId='.$media_object_id.'&byService=publish%7Csharing&token='.$mpx_token.'&account='.$account_id.'&schema=1.0&fields=plprofileresult%24status%2Cplprofileresult%24statusInfo%2Cplprofileresult%24validationErrors%2Cplprofileresult%24profileId&form=json';
            $ch = curl_init($url);
            curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
            curl_setopt($ch, CURLOPT_HEADER, 0);
            $response = curl_exec($ch);
            $responseCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
            curl_close($ch);

            $responseCode = media_theplatform_mpx_check_curl_response_code($url, $responseCode);
            if($responseCode['status'] == 'error') {
              watchdog('media_theplatform_mpx_upload', $responseCode['response'], array(), WATCHDOG_ERROR);
              _mpx_uploads_status_update($publishing_video->id, 'failed', $responseCode['response']);
              continue;
            }

            $response =  media_theplatform_mpx_check_json_response($response, 'ProfileResult');
            if($response['status'] == 'error') {
              watchdog('media_theplatform_mpx_upload', $response['response'], array(), WATCHDOG_ERROR);
              _mpx_uploads_status_update($publishing_video->id, 'failed', $response['response']);
              continue;
            }

            // Inspect each of the publishing profile statuses
            $publishing_profiles = $response['response']->entries;
            $num_publishing_profiles = count($publishing_profiles);
            $num_published = 0;
            foreach($publishing_profiles as $publishing_profile) {
              $status = $publishing_profile->{'plprofileresult$status'};
              $failure_statuses = array('ProcessingError', 'NotProcessed', 'Revoking', 'Revoked', 'RevokingError', 'NotRevoked');
              if(in_array($status, $failure_statuses)) {
                $error_msg = t('MPX publishing error: mediaId: @media_id, profileId: @profile_id, statusInfo: @status_info', array(
                  '@status' => $status,
                  '@media_id' => $media_object_id,
                  '@profile_id' => $publishing_profile->{'plprofileresult$profileId'},
                  '@status_info' => $publishing_profile->{'plprofileresult$statusInfo'}));
                watchdog('media_theplatform_mpx_upload', $error_msg, array(), WATCHDOG_ERROR);
                _mpx_uploads_status_update($publishing_video->id, 'failed', $error_msg);
                // if any publishing profile failed, break the loop for this video
                break;
              } else if($status == 'Processed') {
                // check if all publishing profiles have processed,
                // if so, update the status of the video to finished and
                // trigger the upload publishing finished function.
                if($num_published == $num_publishing_profiles-1) {
                  _mpx_uploads_status_update($publishing_video->id, 'finished');
                  media_theplatform_mpx_upload_published($publishing_video);
                  break;
                }
                $num_published++;
              }
            }
          }
          // Wait 30 seconds before checking again
          sleep(30);
        } else {
          // Release lock and exit if everything is published (or failed)
          lock_release('mpx_publishing_watcher_running');
          break;
        }
      }
    } else {
      // Unable to acquire lock, a watcher is already running, do nothing
      return;
    }
  } else {
    // Unable to log in, report error
    watchdog('media_theplatform_mpx_upload', 'Unable to sign in to MPX account when attempting to poll the publishing status of uploaded videos.', array(), WATCHDOG_ERROR);
  }
}

/**
 * Runs on cron, will ensure that the publishing status watch isn't stuck,
 * and if it is, will set any videos which have been stuck for > 6 hours to failed
 */
function _mpx_publishing_status_watcher_health_check() {
  $publishing_videos = db_select('mpx_uploads', 'q')
    ->fields('q')
    ->condition('status', 'publishing', '=')
    ->execute()
    ->fetchAll();

  foreach($publishing_videos as $publishing_video) {
    $elapsed = REQUEST_TIME - $publishing_video->updated;
    if($elapsed > 21600) {
      _mpx_uploads_status_update($publishing_video->id, 'failed', t('Failed to finish publishing after 6 hours.'));
    }
  }
}

/**
 * Publish a media object using an array of publishing profile IDs
 * @param $media_object_id
 * @param $publish_profiles
 * @return array
 */
function _mpx_publish_media($media_object_id, $publish_profiles) {
  if(media_theplatform_mpx_check_token()) {
    $mpx_token = media_theplatform_mpx_variable_get('token');
    $account_id = media_theplatform_mpx_variable_get('account_id');
    foreach($publish_profiles as $publish_profile) {
      $url = MPX_PUB_BASE.'/web/Publish/publish?form=JSON&schema=1.2&token='.$mpx_token.'&account='.$account_id.'&_mediaId='.$media_object_id.'&_profileId='.$publish_profile;
      $ch = curl_init($url);
      curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
      curl_setopt($ch, CURLOPT_HEADER, 0);
      $response = curl_exec($ch);
      $responseCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
      curl_close($ch);

      $responseCode = media_theplatform_mpx_check_curl_response_code($url, $responseCode);
      if($responseCode['status'] == 'error') {
        return $responseCode;
      }

      $response =  media_theplatform_mpx_check_json_response($response, 'publishMedia');
      if($response['status'] == 'error')
        return $response;
    }
    return array('status'=>'success');
  }
  else {
    return array('status'=>'error', 'response'=>t('Unable to sign in to MPX.'));
  }
}

/**
 * Update the status of an item in the mpx_uploads table
 * @param $mpx_queue_id
 * @param $status
 * @param string $message
 */
function _mpx_uploads_status_update($mpx_queue_id, $status, $message = '') {
  db_update('mpx_uploads')
    ->fields(array(
      'status' => $status,
      'message' => $message,
      'updated' => time(),
    ))
    ->condition('id', $mpx_queue_id, '=')
    ->execute();
}

/**
 * Check the status of an MPX publishing action
 */
function media_theplatform_mpx_publish_status($fileId) {
  return $fileId;
}

/**
 * Finds the MPX-valid format name for a file extension
 * @param $extension
 * @return array
 */
function _mpx_format($extension) {
  if(!$formats = cache_get('mpx_upload:xml_formats')) {
    $formats_XML = simplexml_load_file('http://web.theplatform.com/descriptors/enums/format.xml');
    $formats = array();
    foreach($formats_XML->format as $format) {
      foreach($format->extensions->extension as $ext) {
        $formats[(string) $ext] = (string) $format->title;
      }
    }
    cache_set('mpx_upload:xml_formats', $formats);
  } else {
    $formats = $formats->data;
  }
  if(isset($formats[$extension]))
    return array('status' => 'success', 'response' => $formats[$extension]);
  else
    return array('status' => 'error', 'response' => t('MPX format not found for extension: @extension', array('@extension' => $extension)));
}