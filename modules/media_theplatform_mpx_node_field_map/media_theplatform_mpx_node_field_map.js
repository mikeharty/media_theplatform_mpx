/**
 * @file
=
 */
(function ($) {

  Drupal.behaviors.mediaMpxNodeFieldMap = {
    attach: function (context, settings) {

    },
    copyFields: function () {
      var content_type = parent.Drupal.settings.arg[2];
      var fieldMap = JSON.parse(Drupal.settings.mediaThePlatformMpx.nodeFieldMap);
      fieldMap = fieldMap[content_type];
      for(var field in fieldMap) {
        parent.jQuery('.node-form input, .node-form select').each(function() {
          var parentFieldName = jQuery(this).attr('name');
          if(parentFieldName.substring(0, parentFieldName.indexOf('[')) == field) {
            $('#mpx-upload-form input, #mpx-upload-form select').each(function() {
              var fieldName = $(this).attr('name');
              if(fieldName.substring(0, fieldName.indexOf('[')) == fieldMap[field]) {
                parent.jQuery('[name="'+parentFieldName+'"]').val($(this).val());
              }
            });
          }
        });
      }
    }
  };

}(jQuery));
