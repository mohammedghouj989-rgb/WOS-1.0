" Vim syntax file for WOS
" Language: WOS
" Last Change: 2026

if exists("b:current_syntax")
    finish
endif

" Keywords
syntax keyword wosKeyword int float double bool string void
syntax keyword wosKeyword if else while for switch case
syntax keyword wosKeyword break continue return
syntax keyword wosKeyword struct enum typedef
syntax keyword wosKeyword import const alloc free
syntax keyword wosKeyword print

" GUI Keywords
syntax keyword wosGuiKeyword window button text input label
syntax keyword wosGuiKeyword rectangle circle triangle
syntax keyword wosGuiKeyword center left right top bottom

" Types
syntax keyword wosType int float double bool string void

" Numbers
syntax match wosNumber "\<\d\+\(\.\d\+\)\?\>"

" Strings
syntax region wosString start=+"+ end=+"+ oneline

" Comments
syntax match wosComment "//.*$"

" Operators
syntax match wosOperator "[+\-*/%=<>!&|]"

" Punctuation
syntax match wosPunct "[;,(){}[\]]"

" Highlight groups
highlight link wosKeyword Keyword
highlight link wosGuiKeyword Function
highlight link wosType Type
highlight link wosNumber Number
highlight link wosString String
highlight link wosComment Comment
highlight link wosOperator Operator
highlight link wosPunct Delimiter

let b:current_syntax = "wos"
