<?php
/**
 * @file
 * media_theplatform_mpx/includes/themes/media_theplatform_mpx.theme.inc
 *
 * Preprocess functions for mpx Media magic.
 */

/**
 * Implements hook_PREPROCESS_hook().
 */
function media_theplatform_mpx_preprocess_media_theplatform_mpx_video(&$variables) {
  $account_pid = media_theplatform_mpx_variable_get('account_pid');
  $uri = $variables['uri'];
  $wrapper = file_stream_wrapper_get_instance_by_uri($uri);
  $parts = $wrapper->get_parameters();
  // Player only
  if ($parts['mpx_type'] == 'player') {
    // Retrieve mpx_player data for this file.
    $player = media_theplatform_mpx_get_mpx_player_by_fid($parts['file']->fid);
    $width = $player['width'];
    $height = $player['height'];
    switch(media_theplatform_mpx_variable_get('embed_method', 0)) {
      case 0:
        // HTML/JS
        $body = $player['body_html'];
        break;
      // JS
      case 1:
        $body = '<script src="http://player.theplatform.com/p/'.$account_pid.'/'.$player['pid'].'?form=javascript" type="text/javascript"></script>';
        break;
      // iFrame
      case 2:
        $body = '<iframe src="http://player.theplatform.com/p/'.$account_pid.'/'.$player['pid'].'?form=html" width="'.$width.'" height="'.$height.'" frameBorder="0" seamless="seamless" allowFullScreen></iframe>';
        break;
    }
  }
  // Videos
  if ($parts['mpx_type'] == 'video') {
    // Retrieve mpxPlayer set in URI.
    $player = media_theplatform_mpx_get_mpx_player_by_fid($parts['player_fid']);
    $video = media_theplatform_mpx_get_mpx_video_by_fid($parts['file']->fid);

    if (!$player) {
      $variables['mpx_id'] = NULL;
      $variables['player_html'] = t("A Default mpxPlayer must be selected to view this content.");
      return;
    }

    $release_id = $video['release_id'];
    $width = $player['width'];
    $height = $player['height'];
    switch(media_theplatform_mpx_variable_get('embed_method', 0)) {
      // HTML/JS
      case 0:
        // Alter Default mpxPlayer's code to play this specific mpx_video id.
        $body = media_theplatform_mpx_add_guid_to_html($parts['mpx_id'], $player['body_html']);
        break;
      // JS
      case 1:
        $body = '<div style="width: '.$width.'px; height: '.$height.'px"><script src="http://player.theplatform.com/p/'.$account_pid.'/'.$player['pid'].'/embed/select/'.$release_id.'?form=javascript" type="text/javascript"></script></div>';
        break;
      // iFrame
      case 2:
        $body = '<iframe src="http://player.theplatform.com/p/'.$account_pid.'/'.$player['pid'].'/embed/select/'.$release_id.'?form=html" width="'.$width.'" height="'.$height.'" frameBorder="0" seamless="seamless" allowFullScreen></iframe>';
        break;
    }
  }
  // Add mpx_id's into all of the body HTML.
  $body = media_theplatform_mpx_replace_html_ids($body, $parts['mpx_id']);
  $variables['player_html'] = $body;
  $variables['mpx_id'] = $parts['mpx_id'];
}

/**
 * Table theme for mapping Video File fields to
 * mpx Video fields.
 * @param $vars
 * @return string
 */
function theme_media_theplatform_mpx_file_map_table($vars) {
  $element = $vars['element'];

  $header = array(
    'drupal_field' => t('Video File Field'),
    'mpx_field' => t('mpx Field'),
  );
  $rows = array();
  foreach (element_children($element) as $key) {
    $row = array();

    $row['data'] = array();
    foreach ($header as $fieldname => $title) {
      $row['data'][] = drupal_render($element[$key][$fieldname]);
    }
    $rows[] = $row;
  }

  return theme('table', array(
    'header' => $header,
    'rows' => $rows,
    'attributes' => array('id' => 'form_id'), // needed for table dragging
    'empty' => t('There are no fields available to be mapped. Please check your video file entity fields by <a href="@link">clicking here</a>', array('@link' => '/admin/structure/file-types/manage/video/fields')),
  ));
}
?>
