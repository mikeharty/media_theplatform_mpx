<?php
/**
 * @file
 * Contains MediaThePlatformMpxStreamWrapper.
 *
 * Create an instance like this:
 * $mpx_player = new MediaThePlatformMpxStreamWrapper('mpx://p/[id]);
 * $mpx_video = new MediaThePlatformMpxStreamWrapper('mpx://m/[guid]);
 */

class MediaThePlatformMpxStreamWrapper extends MediaReadOnlyStreamWrapper {

  /**
   *  Returns an array of any parameters stored in the URL's path.
   */
  protected function _parse_url($url) {
    $params = array();

    // Load the File entity for this url into this instance.
    $files = entity_load('file', FALSE, array('uri' => $url));
    $file = !empty($files) ? reset($files) : FALSE;
    $params['file'] = $file;

    $url = parse_url($url);
    $parts = explode('/', $url['path']);
    if ($url['host'] == 'm') {
      $params['mpx_type'] = 'video';
      $default_player = media_theplatform_mpx_get_mpx_player_by_fid(media_theplatform_mpx_variable_get('default_player_fid'));
      $params['player_pid'] = $default_player['pid'];
      if ($file) {
        $this_video = media_theplatform_mpx_get_mpx_video_by_fid($file->fid);
        $params['video_file_url'] = $this_video['file_url'];
      }
    }
    elseif ($url['host'] == 'p') {
      $params['mpx_type'] = 'player';
      if ($file) {
        $this_player = media_theplatform_mpx_get_mpx_player_by_fid($file->fid);
        $params['player_pid'] = $this_player['pid'];
      }
    }
    // Get the string after the mpx://m|p/ to use as filename.
    $params['mpx_id'] = $parts[1];
    return $params;
  }

  function getTarget($f) {
    return FALSE;
  }

  static function getMimeType($uri, $mapping = NULL) {
    return 'video/mpx';
  }

  function getOriginalThumbnailPath() {
    $parts = $this->get_parameters();
    // If this is a video clip:
    if ($parts['mpx_type'] == 'video') {
      // Look up thumbnail and return URL to it.
      return media_theplatform_mpx_get_thumbnail_url($parts['mpx_id']);
    }
    // Else for players, return mpx logo.
    elseif ($parts['mpx_type'] == 'player') {
      global $base_url;
      return $base_url . '/' . drupal_get_path('module', 'media_theplatform_mpx') . '/images/mpx_logo.png';
    }
  }

  function getLocalThumbnailPath() {
    $parts = $this->get_parameters();
    $local_path = 'public://media-mpx/' . $parts['mpx_id'] . '.jpg';
    if (!file_exists($local_path)) {
      $dirname = drupal_dirname($local_path);
      file_prepare_directory($dirname, FILE_CREATE_DIRECTORY | FILE_MODIFY_PERMISSIONS);
      @copy($this->getOriginalThumbnailPath(), $local_path);
    }
    return $local_path;
  }

  function interpolateUrl() {
    if ($params = $this->get_parameters()) {
      if ($params['mpx_type'] == 'video') {
        return media_theplatform_mpx_get_player_url($params['player_pid']) . '/embed/select/' . $params['video_file_url'];
      }
      else {
        return media_theplatform_mpx_get_player_url($params['player_pid']);
      }
    }
  }

}
