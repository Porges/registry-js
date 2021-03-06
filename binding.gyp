{
  'targets': [
    {
      'target_name': 'registry',
      'include_dirs': [ '<!(node -e "require(\'nan\')")' ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'src/main.cc',
          ],
          'msvs_disabled_warnings': [
            4267,  # conversion from 'size_t' to 'int', possible loss of data
            4530,  # C++ exception handler used, but unwind semantics are not enabled
            4506,  # no definition for inline function
          ],
        }],
      ],
    },
    {
      'target_name': 'test',
      'type': 'executable',
      'sources': [
        'src/main.cc',
        'test/test.cpp'
      ],
      'include_dirs': [
        'src',
        '<!(node -e "require(\'cppunitlite\')")',
        '<!(node -e "require(\'nan\')")',
      ],
      'dependencies': [
        'node_modules/cppunitlite/binding.gyp:CppUnitLite'
      ],
      'configurations': {
        'Release': {
          'msvs_settings': {
            'VCCLCompilerTool': {
              'ExceptionHandling': 1
            }
          }
        }
      }
    }
  ]
}
