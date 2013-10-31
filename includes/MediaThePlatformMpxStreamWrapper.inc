<?php
/**
 * @file
 * Contains MediaThePlatformMpxStreamWrapper.
 *
 * Create an instance like this:
 * $mpx_player = new MediaThePlatformMpxStreamWrapper('mpx://p/[id]);
 * $mpx_video = new MediaThePlatformMpxStreamWrapper('mpx://m/[guid]/p/[fid]);
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

    // If mpxMedia:
    if ($url['host'] == 'm') {
      $params['mpx_type'] = 'video';
      // Use mpxMedia's Guid as the mpx_id for output HTML.
      $params['mpx_id'] = $parts[1];
      // The fid of mpxPlayer to render this mpxMedia with.
      $params['player_fid'] = $parts[3];
      // fallback to default.
      if (!$params['player_fid']) {
        $params['player_fid'] = media_theplatform_mpx_variable_get('default_player_fid');
      }
    }

    // Elseif mpxPlayer:
    elseif ($url['host'] == 'p') {
      $params['mpx_type'] = 'player';
      if ($file) {
        // Store this variable so we can consistently just call player_fid when adding css and js to <head>.
        $params['player_fid'] = $file->fid;
        $this_player = media_theplatform_mpx_get_mpx_player_by_fid($file->fid);
        $params['player_pid'] = $this_player['pid'];
        // Use mpxMedia's Pid as the mpx_id for output HTML.
        $params['mpx_id'] = $this_player['pid'];
      }
    }
    return $params;
  }

  function getTarget($f) {
    return FALSE;
  }

  static function getMimeType($uri, $mapping = NULL) {
    $uri_parts = parse_url($uri);
    if ($uri_parts['host'] == 'm') {
      return 'video/mpx';
    }
    else if ($uri_parts['host'] == 'p') {
      return 'player/mpx';
    }
    return '';
  }

  function getOriginalThumbnailPath() {
    $parts = $this->get_parameters();
    // If this is a video clip:
    if ($parts['mpx_type'] == 'video') {
      // Look up thumbnail and return URL to it.
      // Return the encoded url
      return $this->encodePath(media_theplatform_mpx_get_thumbnail_url($parts['mpx_id']));
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
      return url('file/' . $params['file']->fid, array('absolute' => TRUE));
    }
  }

  function encodePath($path) {
    $inputpattern = array("/%3a/i", "/%2f/i");
    $outputpattern = array(":", "/");
    return preg_replace ($inputpattern, $outputpattern, rawurlencode($path));
  }
}
