<?php
/**
 * Form constructor for mpx upload form.
 * This is rendered in the media browser.
 */
function media_theplatform_mpx_upload_form($form, &$form_state) {
  $form['uploadform'] = array(
    '#type' => 'fieldset',
    '#title' => t('Upload video'),
    '#collapsible' => FALSE,
    '#collapsed' => FALSE,
    '#description' => t('Upon submission, this form will start a background task which initiates publishing automatically. Fields which you have mapped are available on this form and will be saved to the created file and MPX.'),
    '#prefix' => '<div id="scrollable">',
    '#suffix' => '</div>',
  );

  // Check that an upload server has been configured
  if(!$upload_server = media_theplatform_mpx_variable_get('upload_server', false)) {
    $form['uploadform']['#description'] = t('Please <a href="@mpxpathadmin" target="_blank">set an upload server</a> to upload mpxVideos.', array('@mpxpathadmin' => '/admin/config/media/theplatform/upload'));
    return $form;
  }

  $form['uploadform']['uploadtitle'] = array(
    '#type' => 'textfield',
    '#required' => true,
    '#title' => t('Title'),
    '#weight' => -100,
    '#description' => t('Video name or title.'),
    '#default_value' => !empty($form_state['values']['title']) ? $form_state['values']['title'] : '',
  );

  $form['uploadform']['filefields'] = array(
    '#tree' => TRUE,
    '#type' => 'fieldset',
    '#collapsed' => false,
    '#title' => t('Synced fields'),
  );

  // Attach the mapped file fields to our form
  $tmpfile = new stdClass();
  $tmpfile->type = 'video';
  field_attach_form('file', $tmpfile, $form['uploadform']['filefields'], $form_state, NULL);
  $field_map = media_theplatform_mpx_variable_get('file_field_map', false);
  if($field_map) {
    $field_map = unserialize($field_map);
    foreach($form['uploadform']['filefields'] as $key => $val) {
      if(strpos($key, '#') !== 0 && !in_array($key, array_keys($field_map)))
        unset($form['uploadform']['filefields'][$key]);
      else if(strpos($key, '#') !== 0)
        $form['uploadform']['filefields'][$key]['#attributes'] = array('data-mpxField' => $field_map[$key]);
    }
  }

  $form['uploadform']['fileupload'] = array(
    '#type' => 'file',
    '#title' => t('Video file'),
    '#size' => 40,
    '#description' => t('Accepted file types: 3g2 3gp asf avi dv flv f4v m4v mov mp4 mpeg mpg mts m2ts qt wmv'),
  );

  $account = media_theplatform_mpx_variable_get('import_account');
  $account_label = str_replace('%', '-', $account);
  $form['uploadform']['upload_player'] = array(
    '#type' => 'select',
    '#title' => t('Render with mpxPlayer:'),
    '#options' => media_theplatform_mpx_get_players_select($account),
    '#default_value' => media_theplatform_mpx_variable_get('default_player_fid'),
    '#prefix' => '<span class="'.$account_label.'">',
    '#suffix' => '</span>',
  );

  $form['uploadform']['publish_profiles'] = array(
    '#type' => 'select',
    '#multiple' => true,
    '#required' => true,
    '#title' => t('Publishing Profiles:'),
    '#options' => media_theplatform_mpx_upload_get_publishing_profiles(),
    '#default_value' => media_theplatform_mpx_variable_get('default_publishing_profiles'),
  );

  $form['uploadform']['media_guid'] = array(
    '#type' => 'hidden',
  );

  $form['uploadform']['media_id'] = array(
    '#type' => 'hidden',
  );

  $form['uploadform']['status'] = array(
    '#markup' => '<div id="upload_status"></div>',
  );

  $form['uploadform']['submit'] = array(
    '#type' => 'button',
    '#name' => 'upload',
    '#value' => t('Upload and attach'),
    '#ajax' => array(
      'callback' => 'ajax_mpx_media_upload_callback',
      'wrapper' => 'mpx-upload-form',
      'event' => 'finish',
    ),
    '#weight' => 1000,
  );
  $form['#prefix'] = '<div id="mpx-upload-form">';
  $form['#suffix'] = '</div>';

  // Add media_theplatform_mpx_upload browser javascript and CSS.
  drupal_add_js(drupal_get_path('module', 'media_theplatform_mpx_upload') . '/js/jquery-1.11.0.min.js');
  drupal_add_js(drupal_get_path('module', 'media_theplatform_mpx_upload') . '/js/rsvp-latest.js');
  drupal_add_js(drupal_get_path('module', 'media_theplatform_mpx_upload') . '/js/media_theplatform_mpx.upload.js');
  drupal_add_js(drupal_get_path('module', 'media_theplatform_mpx_upload') . '/js/thePlatformUpload.js');
  drupal_add_js(
    array(
      'mediaThePlatformMpx' =>
        array(
          'fileFieldMap' => json_encode(unserialize(media_theplatform_mpx_variable_get('file_field_map', 's:0:"";'))),
        ),
        'mediaMpxUpload' =>
        array(
          'accountId' => media_theplatform_mpx_variable_get('account_id'),
          'token' => media_theplatform_mpx_variable_get('token'),
          'uploadServer' => media_theplatform_mpx_variable_get('upload_server'),
        )
    ), 'setting');
  drupal_add_css(drupal_get_path('module', 'media_theplatform_mpx_upload') . '/media_theplatform_mpx.upload.css');
  // Add node field mapping js if enabled
  if(module_exists('media_theplatform_mpx_node_field_map')) {
    drupal_add_js(array('mediaThePlatformMpx' => array('nodeFieldMap' => json_encode(unserialize(media_theplatform_mpx_variable_get('node_field_map', 's:0:"";'))))), 'setting');
    drupal_add_js(drupal_get_path('module', 'media_theplatform_mpx_node_field_map') . '/media_theplatform_mpx_node_field_map.js');
  }
  drupal_add_js(array('mediaThePlatformMpx' => array('copyNodeTitle' => media_theplatform_mpx_variable_get('copy_node_title', true))), 'setting');

  return $form;
}

/**
 * AJAX callback for upload form
 *
 * @param $form
 * @param $form_state
 * @return Validated form with messages
 */
function ajax_mpx_media_upload_callback($form, $form_state) {
  // Pull field values from form
  $title = $form_state['values']['uploadtitle'];
  $fields = media_theplatform_mpx_process_file_form_fields($form_state['values']);
  $publishing_profiles = array_keys($form_state['values']['publish_profiles']);

  // @todo: These need to be added to the form, they will come back from the upload api
  $media_guid = $form_state['values']['media_guid'];
  $media_id = $form_state['values']['media_id'];

  $mpx_queue_id = _mpx_uploads_insert($media_guid, $media_id, $publishing_profiles, $title);

  // If the HTTPRL module is enabled, trigger a non-blocking
  // request to the publishing worker. If not, it will have to wait for cron.
  if(module_exists('httprl')) {
    $trigger_upload_worker = _mpx_publish_worker_trigger($mpx_queue_id);
    if($trigger_upload_worker['status'] == 'error') {
      drupal_set_message($trigger_upload_worker['response'], 'error');
      watchdog('media_theplatform_mpx_upload', t('Unable to reach MPX Upload Queue Worker. Will attempt upload on next cron run.'), array(), WATCHDOG_ERROR);
      return false;
    }
  }

  // Create video file locally, set fields, and insert into MPX tables
  // Insert video function requires an array
  $video_array['id'] = substr($media_id, strrpos($media_id, '/')+1); //extracting the numerical id from the full uri
  $video_array['guid'] =  $media_guid;
  $video_array['title'] = $title;
  $video_array['description'] = '';
  $video_array['thumbnail_url'] = '';
  $video_array['fields'] = $fields;
  $video_array['player_id'] = $form_state['values']['upload_player'];

  media_theplatform_mpx_insert_video($video_array, null);

  $uri = 'mpx://m/' . $video_array['guid'] . '/p/' . $video_array['player_id'];

  $commands = array();
  $commands[] = ajax_command_mpx_media_upload($uri);
  return array('#type' => 'ajax', '#commands' => $commands);
}

/**
 * @param $data
 * @param null $settings
 * @return array
 */
function ajax_command_mpx_media_upload($data, $settings = NULL) {
  return array(
    'command' => 'mpx_media_upload',
    'data' => $data,
    'settings' => $settings,
  );
}