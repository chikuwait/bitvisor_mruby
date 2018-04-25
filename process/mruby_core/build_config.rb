MRuby::Build.new do |conf|
  toolchain :gcc

  enable_debug

  conf.linker do |linker|
    linker.libraries = %w(softfloat)
    linker.library_paths = ["./include/bitvisor"]
  end
  conf.cc.defines << %w(MRB_INT64 SOFTFLOAT_FAST_INT64 LITTLEENDIAN)
end

MRuby::CrossBuild.new('BitVisor') do |conf|
  toolchain :gcc
  conf.linker do |linker|
    linker.libraries = %w(softfloat)
    linker.library_paths = ["./include/bitvisor"]
  end
  conf.gem :core => "mruby-compiler"
  conf.gem :github => "chikuwait/mruby-pack"
  conf.gem :github => "chikuwait/mruby-regexp-pcre"

  conf.cc.include_paths <<  "include/bitvisor"
  conf.cc.flags << "-mno-sse -mno-sse2 -mno-mmx -mno-3dnow -msoft-float -fno-omit-frame-pointer -fno-stack-protector"
  conf.cc.defines << %w(BITVISOR_PROCESS MRB_DISABLE_STDIO MRB_INT64 SOFTFLOAT_FAST_INT64 LITTLEENDIAN)
end
