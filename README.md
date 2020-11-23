# mruby-bin-mrbdump

mrbファイルを読んで人が読める形に出力します。
objdump の真似事です。


## できること

  - `bin/mrbdump -d` :: disassembly
  - -s :: print sections
  - -S :: print irep structures


## くみこみかた

`build_config.rb` に gem として追加して、mruby をビルドして下さい。

```ruby
MRuby::Build.new do |conf|
  conf.gem "mruby-bin-mrbdump", github: "dearblue/mruby-bin-mrbdump"
end
```

- - - -

mruby gem パッケージとして依存したい場合、`mrbgem.rake` に記述して下さい。

```ruby
# mrbgem.rake
MRuby::Gem::Specification.new("mruby-XXX") do |spec|
  ...
  spec.add_dependency "mruby-bin-mrbdump", github: "dearblue/mruby-bin-mrbdump"
end
```


## つかいかた


## Specification

  - Package name: mruby-bin-mrbdump
  - Version: 0.1
  - Product quality: CONCEPT
  - Author: [dearblue](https://github.com/dearblue)
  - Project page: <https://github.com/dearblue/mruby-bin-mrbdump>
  - Licensing: [2 clause BSD License](LICENSE)
  - Dependency external mrbgems: (NONE)
  - Bundled C libraries (git-submodules): (NONE)
