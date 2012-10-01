<?php

/**
 * @file
 * Defines variables and hooks specific to version of Media module.
 */

define('MPX_PATH_ADMIN', 'admin/config/media/theplatform');
define('MPX_PATH_PLAYER', 'admin/content/file/mpxplayer');
define('MPX_PATH_VIDEO', 'admin/content/file/mpxmedia');
define('MPX_PATH_FILE', 'file');

/**
 * Implements hook_media_browser_plugin_info().
 */
function media_theplatform_mpx_media_browser_plugin_info() {
  $info['theplatform_mpx_mpxplayer'] = array(
    'title' => t('mpxPlayers'),
    'class' => 'MediaThePlatformMpxPlayerBrowser',
  );
  $info['theplatform_mpx_mpxmedia'] = array(
    'title' => t('mpxMedia'),
    'class' => 'MediaThePlatformMpxMediaBrowser',
  );

  return $info;
}
