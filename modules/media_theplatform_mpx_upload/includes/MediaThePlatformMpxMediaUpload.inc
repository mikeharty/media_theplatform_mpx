<?php

/**
 * @file
 *
 * Definition of MediaThePlatformMpxMediaBrowser.
 */

/**
 * Media browser plugin for displaying a specific view and display.
 */
class MediaThePlatformMpxMediaUpload extends MediaBrowserPlugin {
  /**
   * Implements MediaBrowserPluginInterface::access().
   */
  public function access($account = NULL) {
    return media_access('edit', $account);
  }

  /**
   * Implements MediaBrowserPlugin::view().
   */
  public function view() {
    if (user_access('administer media')) {
      $build['form'] = drupal_get_form('media_theplatform_mpx_upload_form', 'videos');
      return $build;
    }
  }
}
