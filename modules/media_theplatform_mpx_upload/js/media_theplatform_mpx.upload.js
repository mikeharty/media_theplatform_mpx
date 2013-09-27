/**
 * @file
 * Handles the JS for the Media selector upload form.
 * I'm using name and type selectors throughout this code
 * to deal with Drupal's tendency to increment field IDs
 */
(function ($) {

  Drupal.behaviors.mediaMpxUpload = {
    attach: function (context, settings) {
      // Copy the current node title to the upload title field
      $("#mpx-upload-form input[name=uploadtitle]").val(parent.jQuery(".node-form #edit-title").val());
    }
  };

  Drupal.ajax.prototype.commands.mpx_media_upload = function (ajax, response, status) {
    // Copy the player selection to the selected media field
    var account = $('select[name=upload_player]').closest('span').attr('class');
    $('#media-tab-theplatform_mpx_mpxmedia div.mpxplayer_select.'+account+' select').val($('select[name=upload_player]').val());
    // Select the media
    Drupal.media.browser.selectMedia([{uri: response.data}]);
    $('#media-tab-theplatform_mpx_mpxmedia input[name=selected_file]').val(response.data);
    // Close the browser
    $('#media-tab-theplatform_mpx_mpxmedia .form-actions input[type=submit]').trigger('click');
  };
}(jQuery));
