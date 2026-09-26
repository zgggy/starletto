// 构建脚本：TS 源码 → 类型剥离 → 内联单文件 HTML
// 运行方式：node tools/arch_site/build.ts（Node 24 原生执行 TS）
//
// 离线环境没有 tsc / esbuild，也没有暴露 strip API，因此这里内置一个
// 只覆盖本项目语法子集的确定性剥离器：
//   - 删除 interface 块与 type 别名
//   - 删除「安全位置」的类型注解：用花括号上下文栈区分函数块与对象字面量，
//     冒号是注解当且仅当最内层花括号不是对象字面量——因此对象字面量属性
//     （threshold: 0.08）不受影响，而函数体内的参数/返回注解能被正确删除
//   - 字符串/模板/注释内的内容原样保留
// 产物经过 node --check 语法校验后才写盘。

import { readFileSync, writeFileSync, mkdtempSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { execFileSync } from 'node:child_process'

const ROOT = new URL('.', import.meta.url).pathname
const OUT = join(ROOT, '..', '..', '00_hub', '03_dev', '01_design', 'architecture.html')

function isIdentChar(c: string): boolean {
  return /[A-Za-z0-9_$]/.test(c)
}

function stripTypes(src: string): string {
  const n = src.length
  let out = ''
  let i = 0

  let lineComment = false
  let blockComment = false

  // 花括号上下文栈：'block'（函数/控制流体）或 'object'（对象字面量）。
  // 冒号是「类型注解」当且仅当最内层花括号不是对象字面量——
  // 对象字面量里的属性冒号（threshold: 0.08）因此不会被误删，
  // 而函数体内的参数/返回注解（function(now: number)）能被正确删除。
  const ctx: string[] = []

  const prevNonSpace = function(): string {
    for (let k = out.length - 1; k >= 0; k--) {
      const c = out[k]
      if (c !== ' ' && c !== '\t' && c !== '\n') {
        return c
      }
    }
    return ''
  }

  const atStatementStart = function(): boolean {
    let k = out.length - 1
    while (k >= 0 && (out[k] === ' ' || out[k] === '\t')) {
      k--
    }
    if (k < 0) {
      return true
    }
    return out[k] === '\n'
  }

  const skipString = function(quote: string): void {
    out = out + src[i]
    i = i + 1
    while (i < n) {
      const c = src[i]
      out = out + c
      if (c === '\\') {
        out = out + (src[i + 1] ?? '')
        i = i + 2
        continue
      }
      if (c === quote) {
        i = i + 1
        return
      }
      i = i + 1
    }
  }

  const classifyBrace = function(): string {
    // 依据 out 中最后一个有效 token 判断即将开始的 { 是块还是对象字面量
    let k = out.length - 1
    while (k >= 0 && (out[k] === ' ' || out[k] === '\t' || out[k] === '\n')) {
      k--
    }
    if (k < 0) {
      return 'block'
    }
    const ch = out[k]
    if (ch === ')') {
      return 'block'
    }
    if (ch === '{' || ch === ';') {
      return 'block'
    }
    if (ch === '=') {
      if (k >= 1 && out[k - 1] === '>') {
        return 'block'
      }
      return 'object'
    }
    if (ch === '(' || ch === ',' || ch === '[') {
      return 'object'
    }
    let e = k
    while (k >= 0 && isIdentChar(out[k])) {
      k--
    }
    const w = out.slice(k + 1, e + 1)
    if (w === 'else' || w === 'try' || w === 'do' || w === 'finally') {
      return 'block'
    }
    return 'object'
  }

  while (i < n) {
    const c = src[i]
    const c2 = src.slice(i, i + 2)

    if (lineComment) {
      out = out + c
      if (c === '\n') {
        lineComment = false
      }
      i = i + 1
      continue
    }
    if (blockComment) {
      out = out + c
      if (c2 === '*/') {
        out = out + src[i + 1]
        i = i + 2
        blockComment = false
        continue
      }
      i = i + 1
      continue
    }
    if (c2 === '//') {
      lineComment = true
      out = out + '//'
      i = i + 2
      continue
    }
    if (c2 === '/*') {
      blockComment = true
      out = out + '/*'
      i = i + 2
      continue
    }

    if (c === "'" || c === '"' || c === '`') {
      skipString(c)
      continue
    }

    // interface 块整体删除
    if (c === 'i' && src.slice(i, i + 9) === 'interface' && atStatementStart()) {
      const after = src[i + 9]
      if (after === ' ' || after === '\t') {
        i = i + 9
        while (i < n && src[i] !== '{') {
          i = i + 1
        }
        let depth = 0
        while (i < n) {
          const ch = src[i]
          if (ch === '{') {
            depth = depth + 1
          }
          if (ch === '}') {
            depth = depth - 1
            if (depth === 0) {
              i = i + 1
              break
            }
          }
          i = i + 1
        }
        continue
      }
    }

    // type 别名删除
    if (c === 't' && src.slice(i, i + 5) === 'type ' && atStatementStart()) {
      let depth = 0
      while (i < n) {
        const ch = src[i]
        if (ch === '(' || ch === '[' || ch === '{' || ch === '<') {
          depth = depth + 1
        }
        if (ch === ')' || ch === ']' || ch === '}' || ch === '>') {
          depth = depth - 1
        }
        if (ch === ';' && depth === 0) {
          i = i + 1
          break
        }
        i = i + 1
      }
      continue
    }

    if (c === '{') {
      ctx.push(classifyBrace())
      out = out + c
      i = i + 1
      continue
    }
    if (c === '}') {
      if (ctx.length > 0) {
        ctx.pop()
      }
      out = out + c
      i = i + 1
      continue
    }

    // 类型注解：最内层花括号不是对象字面量时，标识符/)/]/后的冒号视为注解
    const innermost = ctx.length > 0 ? ctx[ctx.length - 1] : 'none'
    if (c === ':' && innermost !== 'object') {
      const prev = prevNonSpace()
      if (isIdentChar(prev) || prev === ')' || prev === ']') {
        i = i + 1
        let d = 0
        while (i < n) {
          const ch = src[i]
          if (ch === "'" || ch === '"' || ch === '`') {
            skipString(ch)
            continue
          }
          if (ch === '(' || ch === '[' || ch === '{' || ch === '<') { d = d + 1 }
          if (ch === ')' || ch === ']' || ch === '}' || ch === '>') {
            if (d === 0) {
              break
            }
            d = d - 1
          }
          if ((ch === ',' || ch === ';' || ch === '=' || ch === '{') && d === 0) {
            break
          }
          i = i + 1
        }
        continue
      }
    }

    out = out + c
    i = i + 1
  }
  return out
}

function checkSyntax(js: string): void {
  const dir = mkdtempSync(join(tmpdir(), 'archsite-'))
  const file = join(dir, 'bundle.js')
  writeFileSync(file, js)
  execFileSync(process.execPath, ['--check', file])
}

function main(): void {
  const ts = readFileSync(join(ROOT, 'main.ts'), 'utf8')
  const tpl = readFileSync(join(ROOT, 'template.html'), 'utf8')

  const js = stripTypes(ts)

  if (js.includes('</script')) {
    throw new Error('剥离产物含有 </script>，会截断内联脚本')
  }
  checkSyntax(js)

  const html = tpl.replace('{{SCRIPT}}', '\n' + js.trim() + '\n')
  writeFileSync(OUT, html)

  const kb = (html.length / 1024).toFixed(1)
  console.log('OK  ' + OUT)
  console.log('    html ' + kb + ' KB · js ' + (js.length / 1024).toFixed(1) + ' KB · node --check 通过')
}

main()
