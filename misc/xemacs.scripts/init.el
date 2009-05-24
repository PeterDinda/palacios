;; .emacs

;;; uncomment this line to disable loading of "default.el" at startup
;; (setq inhibit-default-init t)

(defvar running-xemacs (string-match "XEmacs\\|Lucid" emacs-version))

;; turn on font-lock mode
(when (fboundp 'global-font-lock-mode)
  (global-font-lock-mode t))

;; enable visual feedback on selections
;(setq transient-mark-mode t)

;; default to better frame titles
;;(setq frame-title-format
;;      (concat  "%b - emacs@" system-name))
;; stop at the end of the file, not just add lines
(setq next-line-add-newlines nil)


(cond ((not running-xemacs)
       (global-font-lock-mode t)
))


(when window-system
  ;; enable wheelmouse support by default
  (mwheel-install))

(global-set-key "%" 'match-paren)

      (defun match-paren (arg)
        "Go to the matching parenthesis if on parenthesis otherwise insert %."
        (interactive "p")
        (cond ((looking-at "\\s\(") (forward-list 1) (backward-char 1))
              ((looking-at "\\s\)") (forward-char 1) (backward-list 1))
              (t (self-insert-command (or arg 1)))))


(c-set-offset 'case-label 4)
(setq c-basic-offset 4)

(custom-set-variables
 '(paren-mode (quote sexp) nil (paren))
 '(column-number-mode t)
 '(line-number-mode t)
 '(font-lock-mode t nil (font-lock))
)
(custom-set-faces)



(set-face-background 'default   "Black")
(set-face-foreground 'default   "green")
;;(set-face-foreground 'highlight                    "LightSeaGreen")
;;(set-face-background 'highlight                    "blue")

(set-face-foreground 'font-lock-function-name-face      "White")
(set-face-foreground 'font-lock-keyword-face    "Red")
(set-face-background 'font-lock-keyword-face    "black")
(set-face-foreground 'font-lock-string-face     "LightGoldenRod")
(set-face-foreground 'font-lock-comment-face    "Grey")


(set-face-background 'modeline  "CadetBlue")
(set-face-foreground 'modeline  "black");

