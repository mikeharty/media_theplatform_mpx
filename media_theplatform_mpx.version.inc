<?php

/**
 * @file
 * Defines variables and hooks specific to version of Media module.
 */

define('MPX_PATH_ADMIN', 'admin/config/media/theplatform');
define('MPX_PATH_PLAYER', 'admin/content/media/mpxplayer');
define('MPX_PATH_VIDEO', 'admin/content/media/mpxmedia');
define('MPX_PATH_FILE', 'media');

/**
 * Implements hook_media_browser_plugin_info().
 */
function media_theplatform_mpx_media_browser_plugin_info() {
  $plugins = array();
  if (user_access('administer media')) {
    $plugins['media_theplatform_mpx_videos'] = array(
      '#weight' => -10,
    );
    $plugins['media_theplatform_mpx_players'] = array(
      '#weight' => -10,
    );
  }
  return $plugins;
}

/**
 * Implements hook_media_browser_plugin_view().
 */
function media_theplatform_mpx_media_browser_plugin_view($plugin_name, $params) {
  $path = drupal_get_path('module', 'media');

  $types = isset($params['types']) ? $params['types'] : array();
  $multiselect = isset($params['multiselect']) ? $params['multiselect'] : FALSE;

  $redirect = array('media/browser', array('query' => array('render' => 'media-popup')));
  switch ($plugin_name) {
    case 'media_theplatform_mpx_videos':
      $from_web_form = drupal_get_form('media_theplatform_mpx_form_media_browser', 'videos');
      return array(
        '#title' => t('mpxMedia'),
        'form' => array($from_web_form),
        '#attached' => array(),
      );
    break;
    case 'media_theplatform_mpx_players':
      $from_web_form = drupal_get_form('media_theplatform_mpx_form_media_browser', 'players');
      return array(
        '#title' => t('mpxPlayers'),
        'form' => array($from_web_form),
        '#attached' => array(),
      );
    break;
  }
  return array();
}
