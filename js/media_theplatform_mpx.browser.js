/**
 * @file
 * Handles the JS for the views file browser. Note that this does not currently
 * support multiple file selection
 */


(function ($) {

  Drupal.behaviors.mediaMpxBrowser = {
    attach: function (context, settings) {

      // Container for the files that get passed back to the browser
      var files = {};

      // Disable the links on media items list
      $('ul#media-browser-library-list a').click(function() {
        return false;
      });

      // Catch the click on a media item
      $('.media-item').bind('click', function () {
        // Remove all currently selected files
        $('.media-item').removeClass('selected');
        // Set the current item to active
        $(this).addClass('selected');
        // Add this FID to the array of selected files
        var uri = $(this).parent('a[data-uri]').attr('data-uri');
        // Get the file from the settings which was stored in
        // template_preprocess_media_views_view_media_browser()
        var file = Drupal.settings.media.files[uri];
        var files = new Array();
        files.push(file);
        Drupal.media.browser.selectMedia(files);
        $("input[name='submitted-video']").val(uri);
      });

      $('.mpx-submit').bind('click', function () {
        if ($("input[name='submitted-video']").val() == '') {
          alert(Drupal.t('Please select an item.'));
          return false;
        }
      });          

    }
  }

}(jQuery));
