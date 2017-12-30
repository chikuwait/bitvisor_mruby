MRuby::Build.new do |conf|
  toolchain :gcc

  enable_debug

  conf.linker do |linker|
    linker.libraries = %w(softfloat)
    linker.library_paths = ["./"]
  end
  conf.cc.flags << "-mno-sse -mno-sse2 -mno-mmx -mno-3dnow -msoft-float -fno-omit-frame-pointer"

   conf.cc.defines << %w(MRB_INT64 SOFTFLOAT_FAST_INT64 LITTLEENDIAN)
end
