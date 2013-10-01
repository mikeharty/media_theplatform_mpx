/**
 * @file
 * Checks for a hash time code in the URL and seeks any
 * player on the page to that time when playback begins.
 * e.g. #t=2m or #t=2m30s or #t=120s
 */

(function ($) {
  Drupal.behaviors.mediaMpxDeepLink = {
    scopeSeeks: [],
    attach: function (context, settings) {
      if(window.location.hash) {
        var hash = window.location.hash.substring(1);
        if(hash.indexOf('t=') === 0) {
          var timeCode = hash.substring(hash.indexOf('t=')+2);
          var minIndex = 0;
          var seek = 0;
          if(timeCode.indexOf('m') > 0) {
            minIndex = timeCode.indexOf('m');
            var minutes = timeCode.substring(0, minIndex);
            seek += minutes*60*1000;
          }
          if(timeCode.indexOf('s') > 0) {
            var secondsOffset = 0;
            if(minIndex > 0)
              secondsOffset = minIndex+1;
            var seconds = timeCode.substring(secondsOffset, timeCode.indexOf('s'));
            seek += seconds*1000;
          }
          var that = this;
          $('.media-mpx-wrapper').each(function() {
            var scope = $(this).children('div').attr('id').substring(4);
            if(typeof $pdk !== 'undefined') {
              $pdk.controller.addEventListener("OnMediaStart", "Drupal.behaviors.mediaMpxDeepLink.seek", ["scope-"+scope]);
              that.scopeSeeks[scope] = seek;
            }
          });
        }
      }
    },
    seek: function (evt) {
      var originatorControlId = evt.originator.controlId;
      var scope = originatorControlId.substring(7);
      var seek = Drupal.behaviors.mediaMpxDeepLink.scopeSeeks[scope];
      console.log(seek);
      $pdk.controller.seekToPosition(seek, ["scope-"+scope]);
    }
  }
}(jQuery));