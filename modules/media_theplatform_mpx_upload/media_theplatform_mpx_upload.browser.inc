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
    '#description' => t('We should put some information here about the upload process.'),
    '#prefix' => '<div id="scrollable">',
    '#suffix' => '</div>',
  );

  $form['uploadform']['uploadtitle'] = array(
    '#type' => 'textfield',
    '#title' => t('Title'),
    '#description' => t('Video name or title.'),
    '#default_value' => !empty($form_state['values']['title']) ? $form_state['values']['title'] : '',
  );

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
    '#multiple' => 'true',
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
  $video = _media_theplatform_mpx_upload_form_callback($form, $form_state);
  if (is_array($video) || is_bool($video)) {
    return $video;
  }

  $video_array['id'] = substr($video->id, strrpos($video->id, '/')+1); //extracting the numerical id from the full uri
  $video_array['guid'] = $video->guid;
  $video_array['title'] = $video->title;
  $video_array['description'] = '';
  $video_array['thumbnail_url'] = '';

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

/**
 * Upload the submitted video.
 *
 * @param $form
 * @param $form_state
 * @return bool|StdClass
 */
function _media_theplatform_mpx_upload_form_callback(&$form, $form_state) {
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

  /*
  $meta = array(
    'name' => $form_state['values']['title'],
    'shortDescription' => $form_state['values']['short'],
    'longDescription' => $form_state['values']['long'],
    'linkText' => $form_state['values']['linktext'],
    'linkURL' => $form_state['values']['linkurl'],
    'economics' => $form_state['values']['economics'],
    'referenceId' => brightcove_generate_reference(),
  );

  if (!empty($form_state['values']['tags'])) {
    $meta['tags'] = explode(',', $form_state['values']['tags']);
  }

  if ($custom_fields = variable_get('brightcove_custom_fields', 0)) {
    $meta['customFields'] = array();

    for ($i = 0; $i < $custom_fields; ++$i) {
      $key = $form["custom_field_{$i}"]['#key'];
      $meta['customFields'][$key] = $form_state['values']["custom_field_{$i}"];
    }
  }

  */

  $meta = array(
    'title' => $form_state['values']['uploadtitle'],
  );

  $publishing_profiles = array_keys($form_state['values']['publish_profiles']);

  $upload = media_theplatform_mpx_upload_video(drupal_realpath($file->uri), $meta, $publishing_profiles);

  if ($upload) {
    // Using an object here because it means when we check the result
    // of this method we can filter out errors (boolean) and the form (array)
    $video = new StdClass();
    $video->guid = $upload->guid;
    $video->id = $upload->id;
    $video->title = $upload->meta['title'];
    $video->player = $form_state['values']['upload_player'];
    return $video;
  }

  return $form;
}