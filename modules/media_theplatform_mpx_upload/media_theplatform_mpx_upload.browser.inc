<?php

/**
 * Form constructor for mpx upload form.
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
    '#description' => t('Allowed file types: ') . '<strong>3g2 3gp asf avi dv flv f4v m4v mov mp4 mpeg mpg mts m2ts qt wmv</strong>',
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

  $form['uploadform']['upload'] = array(
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
  return $form;
}

/**
 * Ajax callback for upload form
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
  $fields = _media_theplatform_mpx_process_file_field_form($form_state);
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
  $video_array['id'] = substr($video->id, strrpos($video->id, '/')+1); //extracting the numerical id from the full uri
  $video_array['guid'] = $video->guid;
  $video_array['title'] = $video->title;
  $video_array['description'] = '';
  $video_array['thumbnail_url'] = '';
  $video_array['fields'] = $fields;

  media_theplatform_mpx_insert_video($video_array, null, $video->player);

  $uri = 'mpx://m/' . $video_array['guid'] . '/p/' . $video->player;

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

function _media_theplatform_mpx_process_file_field_form($form) {
  $fields = array();
  if($sync_fields = media_theplatform_mpx_variable_get('file_field_map', false)) {
    foreach(unserialize($sync_fields) as $field => $value) {
      $fi = field_info_field($field);
      switch($fi['type']) {
        // Taxonomy reference fields have a special key
        case 'taxonomy_term_reference':
          $key = 'tid';
          break;
        // @todo: check for other field type keys
        default:
          $key = 'value';
      }
      // Parse multiple value fields
      if(count($form['values'][$field]['und']) > 1) {
        foreach($form['values'][$field]['und'] as $value) {
         $fields[$field][] = $value[$key];
        }
        // single value fields
      } else {
       $fields[$field] = $form['values'][$field]['und'][0][$key];
      }
    }
  }
  return $fields;
}