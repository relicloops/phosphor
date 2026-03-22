import tsParser from '@typescript-eslint/parser';
import tsPlugin from '@typescript-eslint/eslint-plugin';
import perfectionistPlugin from 'eslint-plugin-perfectionist';

export default [
  {
    ignores: ['**/*.js', '**/*.cjs', '**/*.mjs', 'types/**/*', 'docs/**/*', 'scripts/**/*'],
  },
  {
    files: ['**/*.ts', '**/*.tsx'],
    languageOptions: {
      parser: tsParser,
      ecmaVersion: 'latest',
      sourceType: 'module',
    },
    plugins: {
      '@typescript-eslint': tsPlugin,
      perfectionist: perfectionistPlugin,
    },
    rules: {
      'perfectionist/sort-imports': 'error',
      // @typescript-eslint rules
      '@typescript-eslint/no-this-alias': [
        'error',
        { allowedNames: ['bound_data'] },
      ],
      '@typescript-eslint/no-unused-vars': [
        'error',
        {
          "args": "all",
                  "argsIgnorePattern": "^_",
                  "caughtErrors": "all",
                  "caughtErrorsIgnorePattern": "^_",
                  "destructuredArrayIgnorePattern": "^_",
                  "varsIgnorePattern": "^_",
                  "ignoreRestSiblings": true
        },
      ],
      '@typescript-eslint/no-inferrable-types': [
        'error',
        { ignoreParameters: true, ignoreProperties: true },
      ],
      '@typescript-eslint/no-empty-function': [
        'error',
        {
          allow: [
            'arrowFunctions',
            'private-constructors',
            'protected-constructors',
            'decoratedFunctions',
            'overrideMethods',
          ],
        },
      ],
      '@typescript-eslint/ban-ts-comment': [
        'error',
        {
          'ts-ignore': {
            descriptionFormat: '^: @test|@to-fix|@allowed|@reason->',
          },
        },
      ],

      // Core ESLint rules
      'no-multi-spaces': 'error',
      'space-unary-ops': [
        'error',
        { words: true, nonwords: true },
      ],
      quotes: ['error', 'single', { allowTemplateLiterals: true }],
      'computed-property-spacing': ['warn', 'always'],
      'array-bracket-spacing': ['warn', 'always'],
      'space-in-parens': ['warn', 'always'],
      'arrow-spacing': [
        'error',
        { before: true, after: true },
      ],
      'newline-before-return': 'error',
      curly: 'error',
      indent: [
        'warn',
        2,
        { SwitchCase: 1 },
      ],
      'object-curly-spacing': ['warn', 'always'],
      'linebreak-style': ['error', 'unix'],
      semi: ['error', 'always'],
      eqeqeq: ['error', 'always'],
      'no-return-await': 'error',
      'capitalized-comments': 'off',
      'comma-spacing': [
        'warn',
        { before: false, after: true },
      ],
      'no-trailing-spaces': [
        'error',
        { ignoreComments: true },
      ],
      'switch-colon-spacing': 'warn',
      'space-infix-ops': 'error',
      'no-fallthrough': [
        'error',
        { commentPattern: 'break[\\s\\w]*omitted' },
      ],
      'default-case': [
        'error',
        { commentPattern: 'skip\\sdefault' },
      ],
      'prefer-const': [
        'error',
        {
          destructuring: 'all',
          ignoreReadBeforeAssign: true,
        },
      ],
      'no-unused-vars': 'off',
      'no-param-reassign': 'error',
      'arrow-parens': [
        'error',
        'as-needed',
        { requireForBlockBody: true },
      ],
      'brace-style': [
        'error',
        'stroustrup',
        { allowSingleLine: false },
      ],
      'no-unsafe-optional-chaining': 'off',
    },
  },
];
