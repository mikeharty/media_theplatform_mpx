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
    '#description' => t('Upon submission, this form will start a background task which uploads your video and initiates publishing automatically. Fields which you have mapped are available on this form and will be saved to the created file and MPX.'),
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
  );

  $form['uploadform']['submit'] = array(
    '#type' => 'button',
    '#name' => 'upload',
    '#value' => t('Upload and attach'),
    '#ajax' => array(
      'callback' => 'ajax_mpx_media_upload_callback',
      'wrapper' => 'mpx-upload-form',
    ),
    '#weight' => 1000,
  );
  $form['#prefix'] = '<div id="mpx-upload-form">';
  $form['#suffix'] = '</div>';

  // Add media_theplatform_mpx_upload browser javascript and CSS.
  drupal_add_js(drupal_get_path('module', 'media_theplatform_mpx_upload') . '/js/media_theplatform_mpx.upload.js');
  drupal_add_css(drupal_get_path('module', 'media_theplatform_mpx_upload') . '/media_theplatform_mpx.upload.css');
  // Add node field mapping js if enabled
  if(module_exists('media_theplatform_mpx_node_field_map')) {
    drupal_add_js(array('mediaThePlatformMpx' => array('nodeFieldMap' => json_encode(unserialize(media_theplatform_mpx_variable_get('node_field_map', 's:0:"";'))))), 'setting');
    drupal_add_js(drupal_get_path('module', 'media_theplatform_mpx_node_field_map') . '/media_theplatform_mpx_node_field_map.js');
  }

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
  $limits['extensions'] = '3g2 3gp asf avi dv flv f4v m4v mov mp4 mpeg mpg mts m2ts qt wmv';
  $validators = array(
    'file_validate_extensions' => array($limits['extensions']),
  );

  $file = file_save_upload('fileupload', $validators, drupal_realpath(file_default_scheme() . ':/'));

  if (!$file) {
    drupal_set_message(t('Only Video files are allowed here.'), 'error');
    return $form;
  }

  if ($file->filesize <= 0) {
    // Some uploaded files had zero size, that's an error.
    drupal_set_message(t('Uploaded file not found. Are you sure that you uploaded an existing file?'), 'error');
    return $form;
  }

  if (form_get_errors()) {
    return $form;
  }

  // Upload the file to MPX
  $fields = media_theplatform_mpx_process_file_form_fields($form_state['values']);
  $title = $form_state['values']['uploadtitle'];
  $publishing_profiles = array_keys($form_state['values']['publish_profiles']);
  $upload = media_theplatform_mpx_upload_video(drupal_realpath($file->uri), $title, $fields, $publishing_profiles);

  if ($upload) {
    $video = new StdClass();
    $video->guid = $upload->guid;
    $video->id = $upload->id;
    $video->title = $upload->title;
    $video->player = $form_state['values']['upload_player'];
  } else {
    // If the upload failed, it will use DSM to push errors to the page, just return the form
    return $form;
  }

  // Create video file locally, set fields, and insert into MPX tables
  // Insert video function requires an array
  $video_array['id'] = substr($video->id, strrpos($video->id, '/')+1); //extracting the numerical id from the full uri
  $video_array['guid'] = $video->guid;
  $video_array['title'] = $video->title;
  $video_array['description'] = '';
  $video_array['thumbnail_url'] = '';
  $video_array['fields'] = $fields;
  $video_array['player_id'] = $video->player;

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