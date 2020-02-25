(define (problem p02-split-with-cable)
 (:domain entertainment)
 (:objects
  dvd-1 simple-tv-1 scart-to-cinch-cable-1 - equipment
  dvd-1-scart-1 simple-tv-1-cinch-1 simple-tv-1-cinch-2 scart-to-cinch-cable-1-scart-1 scart-to-cinch-cable-1-cinch-2 scart-to-cinch-cable-1-cinch-3 - connector
 )
 (:htn
  :ordered-tasks (and
     (av_connect dvd-1 simple-tv-1)
   )
  :ordering ( )
  :constraints ( ))
 (:init
  ;; device dvd-1
  (conn_of dvd-1 dvd-1-scart-1)
  (unused dvd-1-scart-1)
  (out_connector dvd-1-scart-1)
  (audio_connector dvd-1-scart-1)
  (video_connector dvd-1-scart-1)

  ;; device simple-tv-1
  (conn_of simple-tv-1 simple-tv-1-cinch-1)
  (unused simple-tv-1-cinch-1)
  (in_connector simple-tv-1-cinch-1)
  (video_connector simple-tv-1-cinch-1)
  (conn_of simple-tv-1 simple-tv-1-cinch-2)
  (unused simple-tv-1-cinch-2)
  (in_connector simple-tv-1-cinch-2)
  (audio_connector simple-tv-1-cinch-2)

  ;; device scart-to-cinch-cable-1
  (conn_of scart-to-cinch-cable-1 scart-to-cinch-cable-1-scart-1)
  (unused scart-to-cinch-cable-1-scart-1)
  (out_connector scart-to-cinch-cable-1-scart-1)
  (in_connector scart-to-cinch-cable-1-scart-1)
  (audio_connector scart-to-cinch-cable-1-scart-1)
  (video_connector scart-to-cinch-cable-1-scart-1)
  (conn_of scart-to-cinch-cable-1 scart-to-cinch-cable-1-cinch-2)
  (unused scart-to-cinch-cable-1-cinch-2)
  (out_connector scart-to-cinch-cable-1-cinch-2)
  (in_connector scart-to-cinch-cable-1-cinch-2)
  (audio_connector scart-to-cinch-cable-1-cinch-2)
  (conn_of scart-to-cinch-cable-1 scart-to-cinch-cable-1-cinch-3)
  (unused scart-to-cinch-cable-1-cinch-3)
  (out_connector scart-to-cinch-cable-1-cinch-3)
  (in_connector scart-to-cinch-cable-1-cinch-3)
  (video_connector scart-to-cinch-cable-1-cinch-3)

  ;; compatibility of connections
  (compatible dvd-1-scart-1 scart-to-cinch-cable-1-scart-1)
  (compatible simple-tv-1-cinch-1 scart-to-cinch-cable-1-cinch-2)
  (compatible simple-tv-1-cinch-1 scart-to-cinch-cable-1-cinch-3)
  (compatible simple-tv-1-cinch-2 scart-to-cinch-cable-1-cinch-2)
  (compatible simple-tv-1-cinch-2 scart-to-cinch-cable-1-cinch-3)
  (compatible scart-to-cinch-cable-1-scart-1 dvd-1-scart-1)
  (compatible scart-to-cinch-cable-1-cinch-2 simple-tv-1-cinch-1)
  (compatible scart-to-cinch-cable-1-cinch-2 simple-tv-1-cinch-2)
  (compatible scart-to-cinch-cable-1-cinch-3 simple-tv-1-cinch-1)
  (compatible scart-to-cinch-cable-1-cinch-3 simple-tv-1-cinch-2)
 )
)
