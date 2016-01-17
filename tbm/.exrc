if &cp | set nocp | endif
let s:cpo_save=&cpo
set cpo&vim
imap <F9> :InsPrevHita
imap <F8> :InsNextHita
imap <F5> :PreviewClassa
inoremap <F10> "+gpa
map! <D-v> *
map  h
map <NL> j
map  k
map  l
map   /
vnoremap <silent> # :call VisualSearch('b')
vnoremap $e `>a"`<i"
vnoremap $q `>a'`<i'
vnoremap $$ `>a"`<i"
vnoremap $3 `>a}`<i{
vnoremap $2 `>a]`<i[
vnoremap $1 `>a)`<i(
vnoremap <silent> * :call VisualSearch('f')
map ,q :e ~/buffer
noremap ,m mmHmt:%s///ge'tzt'm
map ,s? z=
map ,sa zg
map ,sp [s
map ,sn ]s
map ,ss :setlocal spell!
map ,u :TMiniBufExplorer:TMiniBufExplorer
map ,p :cp
map ,n :cn
map ,cc :botright cope
map ,cd :cd %:p:h
map ,tm :tabmove 
map ,tc :tabclose
map ,te :tabedit 
map ,tn :tabnew %
map ,ba :1,300 bd!
map ,bd :Bclose
map <silent> , :noh
map ,g :vimgrep // **/*.<Left><Left><Left><Left><Left><Left><Left>
map ,t8 :setlocal shiftwidth=4
map ,t4 :setlocal shiftwidth=4
map ,t2 :setlocal shiftwidth=2
map ,e :e! ~/.vim_runtime/vimrc
nmap ,w :w!
map 0 ^
cmap Â½ $
imap Â½ $
vmap gx <Plug>NetrwBrowseXVis
nmap gx <Plug>NetrwBrowseX
vnoremap <silent> gv :call VisualSearch('gv')
vnoremap <silent> <Plug>NetrwBrowseXVis :call netrw#BrowseXVis()
nnoremap <silent> <Plug>NetrwBrowseX :call netrw#BrowseX(expand((exists("g:netrw_gx")? g:netrw_gx : '<cfile>')),netrw#CheckIfRemote())
map <S-F8> :NextInBlock
map <S-F9> :PrevInBlock
nnoremap <F10> "+gp
vnoremap <F9> "+y
nnoremap <F9> "+yy
map <Left> :bp
map <Right> :bn
map <C-Space> ?
vmap <BS> "-d
vmap <D-x> "*d
vmap <D-c> "*y
vmap <D-v> "-d"*P
nmap <D-v> "*P
cnoremap  <Home>
cnoremap  <End>
cnoremap  
cnoremap  <Down>
cnoremap  <Up>
inoremap $e ""i
inoremap $q ''i
inoremap $4 {o}O
inoremap $3 {}i
inoremap $2 []i
inoremap $1 ()i
cnoremap $q eDeleteTillSlash()
cnoremap $c e eCurrentFileDir("e")
cnoremap $j e ./
cnoremap $d e ~/Desktop/
cnoremap $h e ~/
vmap ë :m'<-2`>my`<mzgv`yo`z
vmap ê :m'>+`<my`>mzgv`yo`z
nmap ë mz:m-2`z
nmap ê mz:m+`z
vmap Â½ $
nmap Â½ $
omap Â½ $
iabbr xdate =strftime("%d/%m/%y %H:%M:%S")
let &cpo=s:cpo_save
unlet s:cpo_save
set autoindent
set autoread
set background=dark
set backspace=eol,start,indent
set cmdheight=2
set comments=s1:/*,mb:*,ex:*/,b:#,:%,:XCOMM,n:>,fb:-
set expandtab
set fileencodings=utf-8,cp936,ucs-bom
set fileformats=unix,dos,mac
set grepprg=/bin/grep\ -nH
set helplang=en
set hidden
set history=300
set hlsearch
set ignorecase
set incsearch
set laststatus=2
set matchtime=2
set mouse=a
set mousemodel=popup
set ruler
set runtimepath=~/.vim,~/.vim/bundle/vim-go,/usr/local/share/vim/vimfiles,/usr/local/share/vim/vim74,/usr/local/share/vim/vimfiles/after,~/.vim/after
set scrolloff=7
set shiftwidth=4
set showmatch
set showtabline=2
set smartindent
set smarttab
set statusline=\ %F%m%r%h\ %w\ \ CWD:\ %r%{CurDir()}%h\ \ \ Line:\ %l/%L:%c
set noswapfile
set switchbuf=usetab
set tabstop=4
set textwidth=500
set updatetime=500
set whichwrap=b,s,<,>,h,l
set wildmenu
set nowritebackup
" vim: set ft=vim :
