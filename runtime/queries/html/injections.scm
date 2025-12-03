; HTML injection query - runtime/queries/html/injections.scm
; This should ONLY capture <script> and <style> blocks, NOT inline attributes

; JavaScript in <script> tags
(script_element
  (raw_text) @injection.content
  (#set! injection.language "javascript"))

; JavaScript in <script type="..."> tags
(script_element
  (start_tag
    (attribute
      (attribute_name) @_attr
      (quoted_attribute_value (attribute_value) @_lang)
      (#eq? @_attr "type")
      (#match? @_lang "^(text/javascript|application/javascript|module)$")))
  (raw_text) @injection.content
  (#set! injection.language "javascript"))

; CSS in <style> tags
(style_element
  (raw_text) @injection.content
  (#set! injection.language "css"))

; CSS in <style type="..."> tags
(style_element
  (start_tag
    (attribute
      (attribute_name) @_attr
      (quoted_attribute_value (attribute_value) @_lang)
      (#eq? @_attr "type")
      (#match? @_lang "^text/css$")))
  (raw_text) @injection.content
  (#set! injection.language "css"))

; REMOVED: Inline event handlers (onclick, onload, etc.)
; These were causing the single-line injections
; We don't inject those because they're too small and cause overlap issues