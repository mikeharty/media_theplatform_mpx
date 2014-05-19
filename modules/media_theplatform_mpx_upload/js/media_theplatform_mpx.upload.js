/**
 * @file
 * Handles the JS for the Media selector upload form.
 * I'm using name and type selectors throughout this code
 * to deal with Drupal's tendency to increment field IDs
 */
jq1110 = $.noConflict(true);

(function ($) {

  Drupal.behaviors.mediaThePlatformMpx = {
    attach: function (context, settings) {
      // Copy the current node title to the upload title field
      var nodeEditTitle = parent.jQuery(".node-form #edit-title").val();
      if(nodeEditTitle != '' && Drupal.settings.mediaThePlatformMpx.copyNodeTitle)
        $("#mpx-upload-form input[name=uploadtitle]").val(nodeEditTitle);

      var me = this;

      $('#media-theplatform-mpx-upload-form').submit(function (e) {
        // @todo: we should probably disable the submit button once this has started
        if(me.uploadFinished == false) {
          e.preventDefault();

          var title = $("#mpx-upload-form input[name=uploadtitle]").val(),
            fields = me.getFieldData(),
            token = Drupal.settings.mediaMpxUpload.token,
            accountId = Drupal.settings.mediaMpxUpload.accountId,
            uploadServer = Drupal.settings.mediaMpxUpload.uploadServer,
            statusCallback = Drupal.behaviors.mediaThePlatformMpx.uploadStatus,
            finishCallback = Drupal.behaviors.mediaThePlatformMpx.finished;

          thePlatformUpload.uploadFile(title, fields, 'edit-fileupload', accountId, token, uploadServer, statusCallback, finishCallback);
          return false;
        }
      });
    },

    buildCategoryList: function($categoryField) {
      var taxTermIds = $categoryField.val();
      var categories = [];


      for(term in taxTermIds) {
        var $option = $categoryField.find('option[value='+taxTermIds[term]+']')
        var textVal = $option.text();
        var category = textVal;
        if(textVal.indexOf('-') === 0) {
          var $prevOptions = $option.prevAll();
          var currentDepth = textVal.split("-").length-1;
          category = category.substr(currentDepth);
          $prevOptions.each(function() {
            var currentText = $(this).text();
            var depth = currentText.split("-").length-1;
            if(depth < currentDepth) {
              category = currentText.substr(currentDepth-1) + '/' + category;
              currentDepth = depth;
            }
            if(currentDepth == 0)
              return false;
          });
        }
        categories.push({'media$name':category});
      }

      return categories;
    },

    getFieldData: function() {
      var me = this;
      var fields = {};

      fields.title = $("#mpx-upload-form input[name=uploadtitle]").val();

      $('[data-mpxfield]').each(function() {
        var $field = $(this).find('input, select, textarea');
        var mpxField = $(this).data('mpxfield');
        if(mpxField == 'categories') {
          if($field.length && $field.val().length)
            fields.categories = me.buildCategoryList($field);
        } else {
          // @todo: test that other fields are being updated
          fields['pl1$'+mpxField] = $field.val();
        }
      });

      return fields;
    },

    uploadFinished: false,

    finished: function(media_guid, media_id) {
      this.uploadFinished = true;
      $('input[name=media_guid]').val(media_guid);
      $('input[name=media_id]').val(media_id);
      jQuery('input[name=upload]').trigger('finish');
    },

    uploadStatus: function(message) {
      $('#upload_status').css('padding-bottom', '5px');
      $('#upload_status').html(message);
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
}(jq1110));
