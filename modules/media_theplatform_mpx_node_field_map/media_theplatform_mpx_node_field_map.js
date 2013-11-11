/**
 * @file
 * If node fields are mapped to file fields, this will
 * copy the values from the node form to the file upload
 * form when it is opened.
 */
(function ($) {
  Drupal.behaviors.mediaMpxNodeFieldMap = {
    attach: function (context, settings) {
      var content_type = parent.Drupal.settings.contentType;
      var fieldMap = JSON.parse(Drupal.settings.mediaThePlatformMpx.nodeFieldMap)[content_type];
      if(fieldMap !== undefined) {
        // For each field in our fieldmap, we need to find the correct field on the node form
        // and on the file upload form. This is a little messy because there's no clean field
        // id attribute on the forms, so we need to parse it out of the field name.
        parent.jQuery('.node-form input, .node-form select').each(function() {
          var parentField = jQuery(this);
          var parentFieldName = jQuery(this).attr('name');
          var parentFieldMapName = parentFieldName.substring(0, parentFieldName.indexOf('['));
          if(fieldMap[parentFieldMapName] != undefined) {
            $('#mpx-upload-form input, #mpx-upload-form select').each(function() {
              var childField = $(this);
              var childFieldName = $(this).attr('name');
              var childFieldMapName = childFieldName.substring(0, childFieldName.indexOf('['))
              if(childFieldMapName == fieldMap[parentFieldMapName]) {
                switch (parentField.attr('type')) {
                  case 'radio':
                  case 'checkbox':
                    if(parentField.is(':checked') && parentField.val() == childField.val()) {
                      childField.attr("checked", "checked");
                      // this breaks the each() if a match is made
                      return false;
                    }
                    break;
                  default:
                    childField.val(parentField.val());
                      // this breaks the each() if a match is made
                    return false;
                }
              }
            });
          }
        });
      }
    }
  };

}(jQuery));
