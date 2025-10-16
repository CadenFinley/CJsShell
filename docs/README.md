# CJ's Shell Documentation

This directory contains the documentation for CJ's Shell, built with [MkDocs](https://www.mkdocs.org/) and the [Material theme](https://squidfunk.github.io/mkdocs-material/).

## Local Development

### Prerequisites

Install the required dependencies:

```bash
pip install -r docs/requirements.txt
```

### Start Development Server

To start the development server with live reload:

```bash
# From the project root
mkdocs serve
```

The documentation will be available at `http://127.0.0.1:8000/CJsShell/`

### Build Documentation

To build the static site:

```bash
mkdocs build
```

The built site will be in the `site/` directory.

## Deployment

Documentation is automatically deployed to GitHub Pages when changes are pushed to the `master` branch. The workflow is defined in `.github/workflows/deploy-docs.yml`.

**Live documentation:** https://cadenfinley.github.io/CJsShell/

## Structure

- `getting-started/` - Installation and setup guides
- `integration/` - Integration with other tools
- `reference/` - Command and API reference
- `scripting/` - Scripting guides and examples
- `themes/` - Theme customization documentation

