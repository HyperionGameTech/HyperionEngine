#!/bin/bash

fix_flag=""
convert=0
files=()
search_dir="Source"

while (( $# )); do
    case "$1" in
        --fix)     fix_flag="--fix --fix-errors"; shift ;;
        --convert) convert=1;                       shift ;;
        --dir)     search_dir="$2";                 shift 2 ;;
        --files)
            shift
            while (( $# )) && [[ $1 != --* ]]; do
                files+=("$1")
                shift
            done
            ;;
        *) shift ;;
    esac
done

# Collect sources if none supplied.
if [ ${#files[@]} -eq 0 ]; then
    while IFS= read -r -d '' file; do
        files+=("$file")
    done < <(find "$search_dir" -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) -print0)
fi

# Optional snake→camel conversion.
if (( convert )); then
    for f in "${files[@]}"; do
        perl -0777 -i -pe '
            BEGIN {
                %kw = map { $_ => 1 } qw(
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
                    $s =~ s/_([a-z])/\U$1/g;
                    return $s;
                }
            }

            s{
                \b
                ( (?:[A-Za-z_][A-Za-z0-9_]*::)* )   # 1: optional qualifier chain
                ( (?:[mgs]_)?[a-z]+(?:_[a-z]+)+ )   # 2: snake_case id (with optional m_/g_/s_)
                \b
            }{
                my ($q, $id) = ($1 // "", $2);

                # Skip if qualifier chain contains std::, or identifier is a keyword,
                # or identifier already has uppercase letters.
                if ($q =~ /\bstd::/ || $kw{$id} || $id =~ /[A-Z]/) {
                    $q . $id;
                }
                # Keep prefix m_/g_/s_ but camel-case the body.
                elsif ($id =~ /^([mgs]_)(.+)$/) {
                    $q . $1 . camel($2);
                }
                # Plain snake_case → camelCase.
                else {
                    $q . camel($id);
                }
            }gex;
        ' "$f"
    done
fi

clang-tidy -p build $fix_flag -format-style=file "${files[@]}" \
  --header-filter=.* \
  --exclude-header-filter="^(.*(thirdparty).*)|(include/.*)" \
  -- -I./thirdparty
