MRuby::Build.new do |conf|
  toolchain :gcc

  enable_debug

  conf.linker do |linker|
    linker.libraries = %w(softfloat)
    linker.library_paths = ["./"]
  end
  conf.cc.defines << %w(SOFTFLOAT_FAST_INT64 LITTLEENDIAN)
end

MRuby::CrossBuild.new('BitVisor') do |conf|
  toolchain :gcc
  conf.linker do |linker|
    linker.libraries = %w(softfloat)
    linker.library_paths = ["./"]
  end
  #conf.cc.gem :core => "mruby-compiler"
  conf.gem :core => "mruby-compiler"
  conf.gem :github => "chikuwait/mruby-pack"
  conf.gem :github => "chikuwait/mruby-regexp-pcre"

  conf.cc.include_paths <<  "include/bitvisor"
  conf.cc.flags << "-mcmodel=kernel -mno-red-zone -mfpmath=387 -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -msoft-float -fno-asynchronous-unwind-tables -fno-omit-frame-pointer -fno-stack-protector"
  conf.cc.defines << %w(MRB_DISABLE_STDIO MRB_INT64 SOFTFLOAT_FAST_INT64 LITTLEENDIAN)
  conf.bins = []
end
