<?php

/**
 * @file media_theplatform_mpx/includes/MediaThePlatformMpxMediaBrowser.inc
 *
 * Definition of MediaThePlatformMpxMediaBrowser.
 */

/**
 * Media browser plugin for displaying a specific view and display.
 */
class MediaThePlatformMpxMediaBrowser extends MediaBrowserPlugin {
  /**
   * Implements MediaBrowserPluginInterface::access().
   */
  public function access($account = NULL) {
    // @todo Replace with file_access().
    return media_access('edit', $account);
  }

  /**
   * Implements MediaBrowserPlugin::view().
   */
  public function view() {
    if (user_access('administer media')) {
      $build['form'] = drupal_get_form('media_theplatform_mpx_form_media_browser', 'videos');
      return $build;
    }
  }
}