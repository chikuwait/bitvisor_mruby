MRuby::Build.new do |conf|
  toolchain :gcc

  enable_debug

  conf.linker do |linker|
    linker.libraries = %w(softfloat)
    linker.library_paths = ["./"]
  end

 # conf.gembox 'default'
end
