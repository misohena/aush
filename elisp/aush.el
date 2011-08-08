
(defvar aush-process nil)
(defvar aush-callback-stopped nil)

(defun aush-process-alived ()
  (and (processp aush-process)
       (eq (process-exit-status aush-process) 0)))

(defun aush-start ()
  (if (not (aush-process-alived))
      (let ((process-connection-type nil)) ;; Use a pipe.
	(setq aush-process (start-process "*aush*" "*aush*" "aush"))
	(process-kill-without-query aush-process t)
	(set-process-filter aush-process 'aush-process-filter)
)))

(defun aush-process-filter (proc str)
  (if (string-match "^stopped." str)
      (progn
	;;(message "sound stopped.")
	(if aush-callback-stopped (funcall aush-callback-stopped)))))


(defun aush-sound-open (filename)
  (aush-start)
  (process-send-string "*aush*" (concat "open " filename "\n")))
(defun aush-sound-close ()
  (if (aush-process-alived)
      (process-send-string "*aush*" "close\n")))
(defun aush-sound-play ()
  (if (aush-process-alived)
      (process-send-string "*aush*" "play\n")))
(defun aush-sound-stop ()
  (if (aush-process-alived)
      (process-send-string "*aush*" "stop\n")))
(defun aush-sound-seekbeg ()
  (if (aush-process-alived)
      (process-send-string "*aush*" "begin\n")))
(defun aush-sound-speed (ratio)
  (if (aush-process-alived)
      (process-send-string "*aush*" (format "speed %f\n" ratio))))

(defun aush-play-file (filename)
  (aush-sound-open filename)
  (aush-sound-play))

(defun aush-set-callback-stopped (func)
  (setq aush-callback-stopped func))


(provide 'aush)
