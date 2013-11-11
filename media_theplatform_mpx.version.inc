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

/**
 * Implements hook_form_alter().
 */
function media_theplatform_mpx_form_alter(&$form, &$form_state, $form_id) {
  // Check if media: single file delete form.
  if ($form['#id'] == 'file-delete-form') {
    $file = file_load($form['fid']['#value']);
    if ($file->filemime == 'player/mpx' && media_theplatform_mpx_get_files_by_player_fid($file->fid)) {
      drupal_set_message(t('@filename cannot be deleted because it is referenced by other mpxMedia Files.', array('@filename' => $file->filename)), 'error');
      unset($form['actions']['submit']);
      unset($form['description']);
    }
  }
  // Check if bulk media delete form.
  elseif ($form['#id'] == 'file-entity-admin-files' && isset($form['operation']) && $form['operation']['#value'] == 'delete') {
    foreach ($form['files'] as $key => $data ) {
      if (is_numeric($key) && isset($data['#value'])) {
        $file = file_load($data['#value']);
        if ($file->filemime == 'player/mpx' && media_theplatform_mpx_get_files_by_player_fid($file->fid)) {
          drupal_set_message(t('@filename cannot be deleted because it is referenced by other mpxMedia Files.', array('@filename' => $file->filename)), 'error');
          unset($form['actions']['submit']);
          unset($form['description']);
        }
      }
    }
  }
}

/**
 * Implements hook_ctools_render_alter().
 */
function media_theplatform_mpx_ctools_render_alter(&$info, &$page, &$context) {
  // Checks for any Media fids in $context array.
  $context_array_values = print_r($context, TRUE);
  $fids = media_theplatform_mpx_extract_fids($context_array_values);

  // Checks for Media fids in Ctools Custom Content which have been saved for re-use.
  // These fids do not appear in the $context array, so we must query ctools_custom_content datatable.
  foreach ($context['handler']->conf['display']->content as $ctools_item) {
    if ($ctools_item->type == 'custom' && $ctools_item->subtype != 'custom') {
      // Query ctools_custom_content to get its saved content.
      $ctools_item_data = db_select('ctools_custom_content', 'cc')
        ->fields('cc')
        ->condition('name', $ctools_item->subtype, '=')
        ->execute()
        ->fetchAssoc();
      // If ctools_custom_content record exists, check its content for any Media files.
      if ($ctools_item_data) {
        $ctools_item_fids = media_theplatform_mpx_extract_fids($ctools_item_data['settings']);
        // If Media Files exist, add them to $fids array.
        if ($ctools_item_fids) {
          $fids = array_merge($fids, $ctools_item_fids);
        }
      }
    }
  }
  media_theplatform_mpx_add_player_head($fids);
}
