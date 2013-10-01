<?php
// Base URLs for MPX endpoints
// Identity management
define('IM_BASE', 'https://identity.auth.theplatform.com/idm');
// File management
define('FM_BASE', 'http://fms.theplatform.com');
// Media data
define('MD_BASE', 'http://data.media.theplatform.com');
// Access/auth
define('AC_BASE', 'http://access.auth.theplatform.com');
// Publish
define('PUB_BASE', 'http://publish.theplatform.com');
// Publishing data
define('PD_BASE', 'http://data.publish.theplatform.com');
// Workflow
define('WF_BASE', 'http://data.workflow.theplatform.com');

/**
 * Fetches a list of publishing profiles for the
 * currently selected MPX account.
 * @return array|bool
 */
function media_theplatform_mpx_upload_get_publishing_profiles() {
  if(media_theplatform_mpx_check_token()) {
    $mpx_token = media_theplatform_mpx_variable_get('token');
    $account_id = media_theplatform_mpx_variable_get('account_id');
    $url = PD_BASE.'/publish/data/PublishProfile?range=1-100&sort=title&count=true&token='.$mpx_token.'&account='.$account_id.'&schema=1.7.0&fields=description%2Cguid%2Cid%2Ctitle%2Cupdated%2Cadded%2CaddedByUserId%2Clocked%2CownerId%2CupdatedByUserId%2Cversion%2Cplprofile%24disabled%2Cplprofile%24fileTargetCount%2Cplprofile%24outletProfileIds%2Cplprofile%24publishProfileIds&form=json';
    $ch = curl_init($url);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_HEADER, 0);
    $response = curl_exec($ch);
    $responseCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);

    // Check for valid cURL response
    $responseCode = _mpx_check_curl_response_code($url, $responseCode);
    if($responseCode['status'] == 'error') {
      drupal_set_message($responseCode['response'], 'error');
      return false;
    }

    $response = _mpx_check_json_response($response, 'getPublishingProfiles');
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
    $url = MD_BASE.'/media/data/Server?sort=title&byOwnerId='.$account_id.'&count=true&token='.$mpx_token.'&schema=1.0&fields=plserver%24disabled%2Clocked%2Ctitle%2Cid&form=json';
    $ch = curl_init($url);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_HEADER, 0);
    $response = curl_exec($ch);
    $responseCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);

    $responseCode = _mpx_check_curl_response_code($url, $responseCode);
    if($responseCode['status'] == 'error') {
      drupal_set_message($responseCode['response'], 'error');
      return false;
    }

    $response = _mpx_check_json_response($response, 'getUploadServer');
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
 * Wrapper function to execute a file upload to MPX.
 * Creates the media object in MPX and triggers the background
 * upload worker.
 * @param $file_path
 * @param $meta
 * @param $publishing_profiles
 * @return bool|stdClass
 */
function media_theplatform_mpx_upload_video($file_path, $meta, $publishing_profiles) {
  // Check that we have a valid token (this will attempt to create one if we don't)
  if(media_theplatform_mpx_check_token()) {
    // Check that an upload server has been configured
    if(!$upload_server = media_theplatform_mpx_variable_get('upload_server')) {
      drupal_set_message(t('Please <a href="@mpxpathadmin">set an upload server</a> to upload media.', array('@mpxpathadmin' => MPX_PATH_ADMIN)), 'error');
      return false;
    }

    // We checked that we have a valid token so these should be set
    $mpx_token = media_theplatform_mpx_variable_get('token');
    $account_id = media_theplatform_mpx_variable_get('account_id');

    // Create the Media Object in MPX
    $media_object = _mpx_create_media_object($account_id, $mpx_token, $meta);
    if($media_object['status'] == 'error') {
      drupal_set_message($media_object['response'], 'error');
      return false;
    }
    else {
      $media_object_id = $media_object['response']->entries[0]->id;
      $media_object_guid = $media_object['response']->entries[0]->guid;
    }

    // Add the video to the upload queue
    $mpx_queue_id = _mpx_uploads_insert($media_object_guid, $file_path, $media_object_id, $upload_server, $publishing_profiles);

    $trigger_upload_worker = _mpx_upload_worker_trigger($mpx_queue_id);
    if($trigger_upload_worker['status'] == 'error') {
      drupal_set_message($trigger_upload_worker['response'], 'error');
      watchdog('media_theplatform_mpx_upload', t('Unable to reach MPX Upload Queue Worker'), array(), WATCHDOG_ERROR);
      _mpx_uploads_status_update($mpx_queue_id, 'failed', $trigger_upload_worker['response']);
      return false;
    }

    // Create a base media object to return to the media browser
    $response = new stdClass();
    $response->guid = $media_object_guid;
    $response->id = $media_object_id;
    $response->meta = $meta;
    return $response;
  } else {
    drupal_set_message(t('Unable to sign in to MPX. Please check your <a href="@mpxpathadmin">set an upload server</a> to upload media.', array('@mpxpathadmin' => MPX_PATH_ADMIN)), 'error');
    return false;
  }
}

/**
 * Inserts an upload into the mpx_uploads table
 * @param $guid
 * @param $file_path
 * @param $media_object_id
 * @param $upload_server
 * @param $publish_profiles
 * @return DatabaseStatementInterface|int|null
 */
function _mpx_uploads_insert($guid, $file_path, $media_object_id, $upload_server, $publish_profiles) {
  $time = time();
  $mpx_queue_id = db_insert('mpx_uploads')
                    ->fields(array(
                      'guid' => $guid,
                      'media_object_id' => $media_object_id,
                      'file_path' => $file_path,
                      'upload_server' => $upload_server,
                      'publish_profiles' => serialize($publish_profiles),
                      'created' => $time,
                      'updated' => $time,
                    ))
                    ->execute();

  _mpx_uploads_status_update($mpx_queue_id, 'new');
  return $mpx_queue_id;
}

/**
 * Triggers a non-blocking HTTP request to the upload queue worker
 * @param $mpx_queue_id
 * @return array
 */
function _mpx_upload_worker_trigger($mpx_queue_id) {
  $resp = array('status' => '', 'message' => '');
  $options = array(
    'blocking' => FALSE,
    'headers' => array(
      // Set the Host header to self.
      'Host' => $_SERVER['HTTP_HOST'],
    ),
  );
  $url = httprl_build_url_self('?q=mpx-worker/upload/'.$mpx_queue_id);
  httprl_request($url, $options);
  httprl_send_request();

  $resp['status'] = 'success';
  return $resp;
}


/**
 * Background process for uploading and publishing MPX videos
 * @param $mpx_queue_id
 */
function media_theplatform_mpx_upload_worker($mpx_queue_id) {
  // This could take a while...
  set_time_limit(0);

  if(media_theplatform_mpx_check_token()) {
    $mpx_token = media_theplatform_mpx_variable_get('token');
    $account_id = media_theplatform_mpx_variable_get('account_id');

    // Fetch upload queue item
    $upload = db_select('mpx_uploads', 'q')
              ->fields('q')
              ->condition('id', $mpx_queue_id, '=')
              ->condition('status', 'new', '=')
              ->execute()
              ->fetchAll();
    $upload = $upload[0];
    $file_path = $upload->file_path;
    $media_object_id = $upload->media_object_id;
    $upload_server = $upload->upload_server;
    $publish_profiles = unserialize($upload->publish_profiles);

    // Update status of upload item in queue
    _mpx_uploads_status_update($mpx_queue_id, 'uploading');

    // Start the upload process, this is synchronous so this thread will be blocked
    $upload = _mpx_upload_video($file_path, $media_object_id, $account_id, $mpx_token, $upload_server);

    if($upload['status'] == 'error') {
      drupal_set_message($upload['response'], 'error');
      watchdog('media_theplatform_mpx_upload', $upload['response'], array(), WATCHDOG_ERROR);
      _mpx_uploads_status_update($mpx_queue_id, 'failed', $upload['response']);
    } else {
      // Upload successful, start publishing
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
    }
  } else {
    watchdog('media_theplatform_mpx_upload', 'Unable to sign in to MPX account when attempting to upload via upload queue worker.', array(), WATCHDOG_ERROR);
  }
}

function _mpx_publishing_status_watcher() {
  if(media_theplatform_mpx_check_token()) {
    $mpx_token = media_theplatform_mpx_variable_get('token');
    $account_id = media_theplatform_mpx_variable_get('account_id');
    // Set a semaphore lock for the running state of this thread.
    // This has an insanely high timeout, because videos can be added to the queue continuously
    // from other threads. There is a safety check, which runs on cron, which will
    // automatically fail any videos which have been publishing for > 6 hours, eventually
    // releasing this thread.
    if(lock_acquire('mpx_publishing_watcher_running', 432000)) {
      watchdog('media_theplatform_mpx_upload', 'MPX Publishing Status Watcher thread started', array(), WATCHDOG_NOTICE);

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
            $url = WF_BASE.'/workflow/data/ProfileResult?byMediaId='.$media_object_id.'&byService=publish%7Csharing&token='.$mpx_token.'&account='.$account_id.'&schema=1.0&fields=plprofileresult%24status%2Cplprofileresult%24statusInfo%2Cplprofileresult%24validationErrors%2Cplprofileresult%24profileId&form=json';
            $ch = curl_init($url);
            curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
            curl_setopt($ch, CURLOPT_HEADER, 0);
            $response = curl_exec($ch);
            $responseCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
            curl_close($ch);

            $responseCode = _mpx_check_curl_response_code($url, $responseCode);
            if($responseCode['status'] == 'error') {
              watchdog('media_theplatform_mpx_upload', $responseCode['response'], array(), WATCHDOG_ERROR);
              _mpx_uploads_status_update($publishing_video->id, 'failed', $responseCode['response']);
              break;
            }

            $response =  _mpx_check_json_response($response, 'ProfileResult');
            if($response['status'] == 'error') {
              watchdog('media_theplatform_mpx_upload', $response['response'], array(), WATCHDOG_ERROR);
              _mpx_uploads_status_update($publishing_video->id, 'failed', $response['response']);
              break;
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
                break 2;
              } else if($status == 'Processed') {
                // check if all publishing profiles have processed,
                // if so, update the status of the video to finished and
                // trigger the upload publishing finished function.
                $num_published++;
                if($num_published == $num_publishing_profiles-1) {
                  _mpx_uploads_status_update($publishing_video->id, 'finished');
                  media_theplatform_mpx_upload_published($publishing_video);
                }
              }
            }
          }
          // Wait 2 minutes before checking again
          sleep(120);
        } else {
          // Release lock and exit if everything is published (or failed)
          lock_release('mpx_publishing_watcher_running');
          die();
        }
      }
    } else {
      // Unable to acquire lock, a watcher is already running, exit
      die();
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
      $url = PUB_BASE.'/web/Publish/publish?form=JSON&schema=1.2&token='.$mpx_token.'&account='.$account_id.'&_mediaId='.$media_object_id.'&_profileId='.$publish_profile;
      $ch = curl_init($url);
      curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
      curl_setopt($ch, CURLOPT_HEADER, 0);
      $response = curl_exec($ch);
      $responseCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
      curl_close($ch);

      $responseCode = _mpx_check_curl_response_code($url, $responseCode);
      if($responseCode['status'] == 'error') {
        return $responseCode;
      }

      $response =  _mpx_check_json_response($response, 'publishMedia');
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
 * Check the status of an MPX upload
 */
function media_theplatform_mpx_upload_status($fileId) {
  return $fileId;
}

/**
 * Creates a new Media Object in MPX
 * @param $account_id
 * @param $mpx_token
 * @param $meta
 * @return array
 */
function _mpx_create_media_object($account_id, $mpx_token, $meta) {
  $media_object_json = '{
    "$xmlns": {
      "media": "http://search.yahoo.com/mrss/",
          "pl": "http://xml.theplatform.com/data/object",
          "plmedia": "http://xml.theplatform.com/media/data/Media"
     },
     "entries": [
        {
          "title": "'.$meta['title'].'",
          "plmedia$approved": true
        }
     ]
  }';

  $url = MD_BASE.'/media/data/Media/list?schema=1.2&form=json&account='.$account_id.'&token='.$mpx_token;
  $ch = curl_init($url);
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
  curl_setopt($ch, CURLOPT_POST, 1);
  curl_setopt($ch, CURLOPT_POSTFIELDS, $media_object_json);
  curl_setopt($ch, CURLOPT_HTTPHEADER, array('Content-Type: text/plain'));
  $response = curl_exec($ch);
  $responseCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
  curl_close($ch);

  $responseCode = _mpx_check_curl_response_code($url, $responseCode);
  if($responseCode['status'] == 'error') {
    return $responseCode;
  }

  return _mpx_check_json_response($response, 'CreateMediaObject');
}

/**
 * Executes the required API calls to upload a video to MPX
 * @param $file_path
 * @param $media_object_id
 * @param $account_id
 * @param $mpx_token
 * @param $upload_server_id
 * @return array
 */
function _mpx_upload_video($file_path, $media_object_id, $account_id, $mpx_token, $upload_server_id) {
  // Prepare the file info
  $file_size = filesize($file_path);
  $path_parts = pathinfo($file_path);
  $file_name = $path_parts['filename'].'.'.$path_parts['extension'];
  $file_extension = $path_parts['extension'];

  // MPX provides a list of file type titles via an XML file
  // This compares the extension on the file to those in the XML
  // and finds the appropriate title
  $mpx_format = _mpx_format($file_extension);
  if($mpx_format['status'] == 'error')
    return $mpx_format;
  else
    $mpx_format = $mpx_format['response'];

  // Generate a GUID for this file upload
  $GUID = uniqid(null, true);

  // Find a random upload server to use for the upload
  $upload_server = _mpx_random_upload_server($account_id, $mpx_token, $upload_server_id);
  if($upload_server['status'] == 'error')
    return $upload_server;
  else
    $upload_server = $upload_server['response'];

  // Create a session with the upload server
  $cookie_file = _mpx_initialize_upload_session($upload_server);
  if($cookie_file['status'] == 'error')
    return $cookie_file;
  else
    $cookie_file = $cookie_file['response'];

  // Create an upload object on the upload server and wait for it to be ready
  $prepare_upload = _mpx_prepare_upload($upload_server, $cookie_file, $mpx_token, $account_id, $GUID, $media_object_id, $file_path, $file_size, $mpx_format, $upload_server_id);
  if($prepare_upload['status'] == 'error')
    return $prepare_upload;

  // Upload the file
  $upload_video = _mpx_upload_video_file($upload_server, $cookie_file, $file_path, $account_id, $mpx_token, $GUID, $file_size);
  if($upload_video['status'] == 'error')
    return $upload_video;

  // Let the upload server know the upload is finished
  $finish_upload = _mpx_finish_upload($upload_server, $cookie_file, $mpx_token, $account_id, $GUID);
  if($finish_upload['status'] == 'error')
    return $finish_upload;

  // Get the mpx file id for the new file
  $uploaded_file_id = _mpx_get_uploaded_file_id($upload_server, $cookie_file, $account_id, $mpx_token, $GUID);
  if($uploaded_file_id['status'] == 'error')
    return $uploaded_file_id;
  else
    $uploaded_file_id = $uploaded_file_id['response'];

  // Return the file id
  return array('status' => 'success', 'response' => $uploaded_file_id);
}

/**
 * Finds a random available upload server on MPX
 * @param $account_id
 * @param $mpx_token
 * @param $upload_server_id
 * @return array
 */
function _mpx_random_upload_server($account_id, $mpx_token, $upload_server_id) {
  // Get a list of available upload servers
  // Pick one at random for pseudo load balancing
  $url = FM_BASE.'/web/FileManagement/getUploadUrls?form=JSON&schema=1.4&token='.$mpx_token.'&account='.$account_id.'&_serverId='.$upload_server_id;
  $ch = curl_init($url);
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
  curl_setopt($ch, CURLOPT_HEADER, 0);
  $response = curl_exec($ch);
  $responseCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
  curl_close($ch);

  $responseCode = _mpx_check_curl_response_code($url, $responseCode);
  if($responseCode['status'] == 'error') {
    return $responseCode;
  }
  $response =  _mpx_check_json_response($response, 'getUploadUrls');
  if($response['status'] == 'error')
    return $response;
  else {
    $response = $response['response'];
    $random_array_key = array_rand($response->getUploadUrlsResponse, 1);
    $upload_server = $response->getUploadUrlsResponse[$random_array_key];
    return array('status' => 'success', 'response' => $upload_server);
  }
}

/**
 * Initializes session with MPX upload server
 * @param $upload_server
 * @return array
 */
function _mpx_initialize_upload_session($upload_server) {
  // Establish session with upload server
  // A cookie needs to be created locally for subsequent requests
  $cookie_file = tempnam ('/tmp', 'CURLCOOKIE');
  $url = $upload_server.'/crossdomain.xml';
  $ch = curl_init($url);
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
  curl_setopt($ch, CURLOPT_COOKIEJAR, $cookie_file);
  curl_setopt($ch, CURLOPT_HEADER, 0);
  curl_exec($ch);
  $responseCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
  curl_close($ch);

  $responseCode = _mpx_check_curl_response_code($url, $responseCode);
  if($responseCode['status'] == 'error') {
    return $responseCode;
  }

  return array('status' => 'success', 'response' => $cookie_file);
}

/**
 * Creates an upload object on the MPX upload server and then polls
 * the upload server for the state of the upload object until it is ready.
 * @param $upload_server
 * @param $cookie_file
 * @param $mpx_token
 * @param $account_id
 * @param $GUID
 * @param $media_object_id
 * @param $file_path
 * @param $file_size
 * @param $format
 * @param $upload_server_id
 * @return array
 */
function _mpx_prepare_upload($upload_server, $cookie_file, $mpx_token, $account_id, $GUID, $media_object_id, $file_path, $file_size, $format, $upload_server_id) {
  // PUT to startUpload to get upload object created
  $url = $upload_server.'/web/Upload/startUpload?schema=1.1&token='.$mpx_token.'&account='.$account_id.'&_guid='.$GUID.'&_mediaId='.$media_object_id.'&_filePath='.$file_path.'&_fileSize='.$file_size.'&_mediaFileInfo.format='.$format.'&_serverId='.$upload_server_id;
  $ch = curl_init($url);
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
  curl_setopt($ch, CURLOPT_COOKIEJAR, $cookie_file);
  curl_setopt($ch, CURLOPT_COOKIEFILE, $cookie_file);
  curl_setopt($ch, CURLOPT_PUT, 1);
  curl_setopt($ch, CURLOPT_HEADER, 0);
  $response = curl_exec($ch);
  $responseCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
  curl_close($ch);

  $responseCode = _mpx_check_curl_response_code($url, $responseCode);
  if($responseCode['status'] == 'error') {
    return $responseCode;
  }

  // If successful, this will return empty
  $response = _mpx_check_empty_response($response, 'startUpload');
  if($response['status'] == 'error')
    return $response;

  // Poll the state of the upload object until it is ready
  // Keep track of how long we've been waiting and timeout after 20 seconds
  $i = 0;
  while(1) {
    $url = $upload_server.'/data/UploadStatus?schema=1.0&account='.$account_id.'&token='.$mpx_token.'&byGuid='.$GUID;
    $ch = curl_init($url);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt ($ch, CURLOPT_COOKIEFILE, $cookie_file);
    curl_setopt($ch, CURLOPT_HEADER, 0);
    $response = curl_exec($ch);
    $responseCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);

    $responseCode = _mpx_check_curl_response_code($url, $responseCode);
    if($responseCode['status'] == 'error') {
      return $responseCode;
    }

    $response = _mpx_check_json_response($response, 'UploadStatus');
    if($response['status'] == 'error')
      return $response;
    $state = $response['response']->entries[0]->state;

    // Still waiting
    if($state != 'Ready') {
      // Timeout check
      if($i > 20)
        return array('status' => 'error', 'response' => t('Timed out while waiting for upload object to be created on MPX upload server.'));
      // Sleep for 1 second
      sleep(1);
      $i++;
    }
    // Ready for upload
    else {
      return array('status' => 'success');
    }

  }
}

/**
 * Uploads a file to MPX in 5mb fragments
 * @param $upload_server
 * @param $cookie_file
 * @param $file_path
 * @param $account_id
 * @param $mpx_token
 * @param $GUID
 * @param $file_size
 * @return array
 */
function _mpx_upload_video_file($upload_server, $cookie_file, $file_path, $account_id, $mpx_token, $GUID, $file_size) {
  // Size of fragments we will send in bytes
  $fragment_size = 5000000;
  // Number of fragments (Round up file_size/fragment_size)
  $fragments = (int) ceil($file_size/$fragment_size);
  // Start reading the file at 0;
  $offset = 0;
  // File handle
  $file_handle = fopen($file_path, 'r');
  fseek($file_handle, $offset);
  for($i = 1; $i <= $fragments; $i++) {
    // If we are on the last fragment, reduce the fragment size to the remaining bytes
    if($i === $fragments) {
      $fragment_size = $file_size % $fragment_size;
    }

    $url = $upload_server.'/web/Upload/uploadFragment?schema=1.1&token='.$mpx_token.'&account='.$account_id.'&_guid='.$GUID.'&_offset='.$offset.'&_size='.$fragment_size;
    $ch = curl_init($url);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_COOKIEFILE, $cookie_file);
    curl_setopt($ch, CURLOPT_PUT, 1);
    curl_setopt($ch, CURLOPT_BINARYTRANSFER, true);
    curl_setopt($ch, CURLOPT_INFILE, $file_handle);
    curl_setopt($ch, CURLOPT_INFILESIZE, $fragment_size);
    curl_setopt($ch, CURLOPT_HEADER, 0);
    $response = curl_exec($ch);
    $responseCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);

    $responseCode = _mpx_check_curl_response_code($url, $responseCode);
    if($responseCode['status'] == 'error') {
      return $responseCode;
    }

    // This should return empty
    $response = _mpx_check_empty_response($response, 'uploadFragment');
    if($response['status'] == 'error')
      return $response;

    // Increase offset by fragment size
    $offset+=$fragment_size;
    // If we're not at the end of the file, seek by offset
    if($i !== $fragments)
      fseek($file_handle, $offset);
  }

  fclose($file_handle);
  return array('status' => 'success');
}

/**
 * Tells the MPX upload server that all upload fragments have been sent
 * @param $upload_server
 * @param $cookie_file
 * @param $mpx_token
 * @param $account_id
 * @param $GUID
 * @return array
 */
function _mpx_finish_upload($upload_server, $cookie_file, $mpx_token, $account_id, $GUID) {
  // Close the Upload process
  $url = $upload_server.'/web/Upload/finishUpload?schema=1.1&token='.$mpx_token.'&account='.$account_id.'&_guid='.$GUID;
  $ch = curl_init($url);
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
  curl_setopt($ch, CURLOPT_COOKIEFILE, $cookie_file);
  curl_setopt($ch, CURLOPT_POST, 1);
  curl_setopt($ch, CURLOPT_POSTFIELDS, 'finished');
  curl_setopt($ch, CURLOPT_HTTPHEADER, array('Content-Type: application/x-www-form-urlencoded'));
  $resp = curl_exec($ch);
  $responseCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
  curl_close($ch);

  $responseCode = _mpx_check_curl_response_code($url, $responseCode);
  if($responseCode['status'] == 'error') {
    return $responseCode;
  }

  // Should return nothing
  return _mpx_check_empty_response($resp, 'finishUpload');
}

/**
 * Finds the MPX valid format name for a file extension
 * @param $extension
 * @return array
 */
function _mpx_format($extension) {
  $mpx_format = null;
  // @todo: Cache this XML file
  $formats_XML = simplexml_load_file('http://web.theplatform.com/descriptors/enums/format.xml');
  foreach($formats_XML->format as $format) {
    foreach($format->extensions->extension as $ext) {
      if($extension == $ext) {
        $mpx_format = $format->title;
        break;
      }
    }
    if($mpx_format != null)
      return array('status' => 'success', 'response' => $mpx_format);
  }
  return array('status' => 'error', 'response' => t('MPX format not found for extension: @extension', array('@extension' => $extension)));
}

/**
 * Gets the ID of a file after it has been uploaded to MPX
 * There maybe be some delay, so this polls every second
 * @param $upload_server
 * @param $cookie_file
 * @param $account_id
 * @param $mpx_token
 * @param $GUID
 * @return array
 */
function _mpx_get_uploaded_file_id($upload_server, $cookie_file, $account_id, $mpx_token, $GUID) {
  // Poll the state of the upload object until it's state is complete
  // Keep track of the seconds passed, timeout after 120 seconds
  $i = 0;
  while(1) {
    $url = $upload_server.'/data/UploadStatus?schema=1.0&account='.$account_id.'&token='.$mpx_token.'&byGuid='.$GUID;
    $ch = curl_init($url);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt ($ch, CURLOPT_COOKIEFILE, $cookie_file);
    curl_setopt($ch, CURLOPT_HEADER, 0);
    $response = curl_exec($ch);
    $responseCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);

    $responseCode = _mpx_check_curl_response_code($url, $responseCode);
    if($responseCode['status'] == 'error') {
      return $responseCode;
    }

    $response = _mpx_check_json_response($response, 'UploadStatus');
    if($response['status'] == 'error')
      return $response;
    else
      $response = $response['response'];

    $state = $response->entries[0]->state;

    if($state == 'Complete') {
      $uploaded_file_id = $response->entries[0]->fileId;
      break;
    }
    else {
      // timeout
      if($i == 120)
        return array('status' => 'error', 'response' => t('Timeout while waiting for UploadStatus'));
      else {
        sleep(2);
        $i++;
      }
    }
  }
  return array('status' => 'success', 'response' => $uploaded_file_id);
}

/**
 * Checks for a valid JSON response from an MPX API call
 * @param $response
 * @param $service
 * @return array
 */
function _mpx_check_json_response($response, $service) {
  // No response
  if(!strlen($response))
    return array('status' => 'error', 'response' => t('No response from @service', array('@service', $service)));
  // Decode JSON
  $responseObject = json_decode($response);
  // Make sure the response decodes, if not, return it's text
  if(!is_object($responseObject))
    return array('status' => 'error', 'response' => t('Error response from @service: @response', array('@service' => $service, '@response' => $response)));
  // Check for an exception on the response, return it's description if set
  if(property_exists($responseObject, 'isException'))
    return array('status' => 'error', 'response' => t('Exception from @service: @response', array('@service' => $service, '@response' => $responseObject->description)));
  // Looking good, return the response object
  else
    return array('status' => 'success', 'response' => $responseObject);
}

/**
 * Checks for an empty response from an MPX API call
 * (as opposed to an error response)
 * @param $response
 * @param $service
 * @return array
 */
function _mpx_check_empty_response($response, $service) {
  // No response (this is what we want in this case)
  if(!strlen($response))
    return array('status' => 'success');

  // If there is a response, look for an exception
  $responseObject = json_decode($response);
  // Make sure the response decodes, if not, return it's text
  if(!is_object($responseObject))
    return array('status' => 'error', 'response' => t('Error response from @service: @response', array('@service' => $service, '@response' => $response)));
  // Check for an exception on the response, return it's description if set
  if(property_exists($responseObject, 'isException'))
    return array('status' => 'error', 'response' => t('Exception from @service: @response', array('@service' => $service, '@response' => $responseObject->description)));
}

function _mpx_check_curl_response_code($url, $responseCode) {
  if(strpos($responseCode, '2') === 0) {
    return array('status' => 'success');
  }
  else {
    return array('status' => 'error', 'response' => t('MPX cURL request to: @url failed with response code: @responseCode', array('@url' => $url, '@responseCode' => $responseCode)));
  }
}