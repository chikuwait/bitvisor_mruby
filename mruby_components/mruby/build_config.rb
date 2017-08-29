MRuby::Build.new do |conf|
  toolchain :gcc

  enable_debug

  conf.linker do |linker|
    linker.libraries = %w(softfloat)
    linker.library_paths = ["./"]
  end
 # conf.gembox 'default'
end
MRuby::CrossBuild.new('BitVisor') do |conf|
   toolchain :gcc
   conf.linker do |linker|
    linker.libraries = %w(softfloat)
    linker.library_paths = ["./"]
   end

   conf.cc.include_paths << "include/bitvisor"
   conf.cc.flags << "-mcmodel=kernel -mno-red-zone -mfpmath=387 -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -msoft-float -fno-asynchronous-unwind-tables -fno-omit-frame-pointer -fno-stack-protector"
   conf.cc.defines << %w(DISABLE_STDIO)
   conf.cc.defines << %w(DISABLE_FLOAT)
   conf.cc.defines << %w(MRB_INT64)
   conf.bins = []
end

