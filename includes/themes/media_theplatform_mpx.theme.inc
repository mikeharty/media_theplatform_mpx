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
  $uri = $variables['uri'];
  $wrapper = file_stream_wrapper_get_instance_by_uri($uri);
  $parts = $wrapper->get_parameters();

  if ($parts['mpx_type'] == 'player') {
    // Retrieve mpx_player data for this file.
    $player = media_theplatform_mpx_get_mpx_player_by_fid($parts['file']->fid);
    $body = $player['body_html'];
  }
  if ($parts['mpx_type'] == 'video') {
    // Retrieve default Player.
    $player = media_theplatform_mpx_get_mpx_player_by_fid(media_theplatform_mpx_variable_get('default_player_fid'));
    if (!$player) {
      $variables['mpx_id'] = NULL;
      $variables['player_html'] = t("A Default mpx Player must be selected to view this content.");
      return;
    }
    // Alter Default Player's code to play this specific mpx_video id.
    $body = media_theplatform_mpx_add_guid_to_html($parts['mpx_id'], $player['body_html']);
  }
  // Add mpx_id's into all of the body HTML.
  $body = media_theplatform_mpx_replace_html_ids($body, $parts['mpx_id']);
  // Grab css data from the head_html field.
  $css = media_theplatform_mpx_get_mpx_player_css($player['head_html']);
  // Replace its selectors.
  $css = media_theplatform_mpx_replace_css_ids($css, $parts['mpx_id']);
  // Add inline/external JS.
  $inline_js = media_theplatform_mpx_get_mpx_player_js($player['head_html']);
  // Render all css and player body HTML together.
  $html = media_theplatform_mpx_render_mpx_player($parts['mpx_id'], $css, $body);
  $variables['player_html'] = $inline_js . $html;
  $variables['mpx_id'] = $parts['mpx_id'];
}
