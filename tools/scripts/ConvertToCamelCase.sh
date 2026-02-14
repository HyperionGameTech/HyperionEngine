#!/bin/bash
# Script to convert snake_case identifiers to CamelCase in C++ source files.

convert=0
files=()
exclude_patterns=()
search_dir="codegen"

# ── arg parsing ──────────────────────────────────────────────────────────────
while (( $# )); do
    case "$1" in
        --convert) convert=1; shift ;;
        --dir) search_dir="$2"; shift 2 ;;
        --exclude)
            shift
            while (( $# )) && [[ $1 != --* ]]; do
                exclude_patterns+=("$1"); shift
            done ;;
        --files)
            shift
            while (( $# )) && [[ $1 != --* ]]; do
                files+=("$1"); shift
            done ;;
        *) shift ;;
    esac
done

# ── collect source files when none given -------------------------------------
if [ ${#files[@]} -eq 0 ]; then
    while IFS= read -r -d '' f; do
        skip=0
        for p in "${exclude_patterns[@]}"; do
            [[ $f == *"$p"* ]] && { skip=1; break; }
        done
        (( skip )) || files+=("$f")
    done < <(find "$search_dir" -type f \( \
        -name '*.cpp' -o -name '*.hpp' -o -name '*.h' -o -name '*.inl' -o -name '*.natvis' \
    \) -print0)
fi

# ── conversion pass ----------------------------------------------------------
if (( convert )); then
    total=${#files[@]}
    bar_len=20
    idx=0

    for f in "${files[@]}"; do
        (( idx++ ))
        perl -0777 -i -pe '
            use strict;
            use warnings;
            our %kw = map { $_ => 1 } qw(
                alignas alignof asm auto bool break case catch char char8_t char16_t char32_t
                class concept const consteval constexpr constinit const_cast continue co_await
                co_return co_yield decltype default delete do double dynamic_cast else enum
                explicit export extern false float for friend goto if inline int long mutable
                namespace new noexcept nullptr operator private protected public register
                reinterpret_cast requires return short signed sizeof static static_assert
                static_cast struct switch template this thread_local throw true try typedef
                typeid typename union unsigned using virtual void volatile wchar_t while
            );
            sub camel {
                my $s = shift;
                $s =~ s/_([a-z0-9])/\U$1/g;
                return $s;
            }
            s{
                (?:
                    "(?:\\.|[^"\\])*"
                  | R"[^()\s]{0,16}\((?:.|\n)*?\)[^"]*"
                ) (*SKIP)(*F)
                |
                (?<!<)
                \b
                (?<qual>(?:[A-Za-z_][A-Za-z0-9_]*::)*)
                (?<id>(?:[mgs]_)?[a-z][a-z0-9]*(?:_[a-z0-9]+)+)
                \b
            }{
                my ($q,$id) = ($+{qual}, $+{id});
                if ( # handle false positives, keywords, and special cases
                       $q =~ /\bstd::/
                    || $kw{$id}
                    || $id eq "c_str"
                    || $id =~ /[A-Z]/
                    || $id =~ /_t$/
                    || $id =~ /^less_equal$/
                    || $id =~ /^greater_equal$/
                    || $id =~ /^tuple_element$/
                    || $id =~ /^tuple_size$/
                    || $id =~ /^always_inline$/
                    || $id =~ /^warn_unused_result$/
                    || $id =~ /^aligned_alloc$/
                    || $id =~ /^notify_all$/
                    || $id =~ /^notify_one$/
                    || $id =~ /^fetch_(add|sub|and|or|xor|min|max)$/
                    || $id =~ /^is_enum_v$/
                    || $id =~ /_v$/
                    || $id =~ /^curl_/
                    || $id =~ /^pthread_/
                    || $id =~ /^st_/
                    || $id =~ /^tv_/
                    || $id =~ /^va_/
                ) {
                    $q.$id;
                } elsif ($id =~ /^([mgs]_)(.+)$/) {
                    $q.$1.camel($2);
                } else {
                    $q.camel($id);
                }
            }gexm;
        ' "$f"

        percent=$(( idx * 100 / total ))
        filled=$(( percent * bar_len / 100 ))
        bar=$(printf '%*s' "$filled" '' | tr " " '#')
        printf "\r\033[K[%d/%d] [%-*s] %3d%% %s" \
               "$idx" "$total" "$bar_len" "$bar" "$percent" "$f"
    done
    echo -e "\nDone – processed $total file(s)."
else
    echo "Nothing to do (run with --convert)."
fi
