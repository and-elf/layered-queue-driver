# GitHub Native Code Coverage Setup

This project uses **GitHub Actions native features** for code coverage, without external apps like Codecov or Coveralls.

## How It Works

### 1. Coverage Workflow (`.github/workflows/test.yml`)

On every push and PR:
- Builds project with coverage instrumentation (`-DENABLE_COVERAGE=ON`)
- Runs all tests
- Generates coverage report using `lcov`
- Adds coverage summary to **GitHub Actions Summary**
- Posts coverage comment on **Pull Requests**
- Uploads HTML coverage report as **artifact**

**Features:**
- ✅ No external services required
- ✅ Coverage displayed directly in PR comments
- ✅ Job summary shows coverage in Actions UI
- ✅ Downloadable HTML reports
- ✅ Coverage threshold checking

### 2. Coverage Badge (`.github/workflows/coverage-badge.yml`)

On every push to `main`:
- Generates coverage percentage
- Creates shields.io-compatible JSON badge
- Commits badge data to `.github/badges/coverage.json`
- Badge auto-updates in README

**Badge URL format:**
```markdown
[![Coverage](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/USERNAME/REPO/main/.github/badges/coverage.json)](LINK)
```

### 3. Local Coverage Script (`scripts/generate_coverage.sh`)

Run locally to generate detailed coverage reports:
```bash
./scripts/generate_coverage.sh
```

Output:
- HTML report in `build/coverage/index.html`
- Terminal summary with color-coded percentages
- Automatically opens in browser

## Viewing Coverage

### On GitHub (Pull Requests)

1. **PR Comment**: Coverage bot posts detailed report
   - Overall percentage
   - File-by-file breakdown
   - Auto-updates on new commits

2. **Actions Summary**: Click on workflow run
   - Scroll to coverage job summary
   - See formatted coverage report

3. **Artifacts**: Download full HTML report
   - Go to workflow run
   - Download `coverage-report` artifact
   - Extract and open `index.html`

### On GitHub (Main Branch)

- **README Badge**: Shows current coverage %
- **Badge updates automatically** after each merge

### Locally

```bash
# Generate and view
./scripts/generate_coverage.sh

# Or manually open
xdg-open build/coverage/index.html
```

## Configuration

### Coverage Threshold

Edit `.github/workflows/test.yml`:
```yaml
- name: Check coverage threshold
  run: |
    THRESHOLD=70  # Adjust this value
```

Threshold options:
- **70%** (current): Warning if below
- **80%**: Production-grade threshold
- **90%**: High-quality target

### Badge Colors

Automatic color coding:
- 🟢 **Green**: ≥80% coverage
- 🟡 **Yellow**: 60-79% coverage  
- 🔴 **Red**: <60% coverage

Edit in `.github/workflows/coverage-badge.yml` to adjust thresholds.

### Coverage Filters

Exclude files from coverage in `.github/workflows/test.yml`:
```bash
lcov --remove coverage.info \
  '/usr/*' \           # System headers
  '*/tests/*' \        # Test code
  '*/googletest/*' \   # Test framework
  '*/build/_deps/*' \  # Dependencies
  '*/samples/*' \      # Example code
  --output-file coverage.info
```

Add more patterns as needed:
```bash
  '*/generated/*' \    # Generated code
  '*/mock/*' \         # Mocks
```

## Permissions

The workflows require these permissions:

**`test.yml` (PR coverage):**
```yaml
permissions:
  contents: read
  pull-requests: write  # To post comments
```

**`coverage-badge.yml` (badge updates):**
```yaml
permissions:
  contents: write  # To commit badge JSON
```

## Troubleshooting

### Badge not updating

1. Check workflow ran successfully on `main` branch
2. Verify `.github/badges/coverage.json` exists in repo
3. Badge URL must point to `raw.githubusercontent.com`
4. GitHub may cache badge for 5 minutes

### Coverage report empty

1. Ensure tests are actually running (`ctest --output-on-failure`)
2. Check build has `-DENABLE_COVERAGE=ON`
3. Verify `lcov` installed in CI environment
4. Check coverage filters aren't too aggressive

### PR comment not posting

1. Verify workflow has `pull-requests: write` permission
2. Check GitHub Actions bot has repo access
3. Look for script errors in workflow logs

## Advantages Over External Services

| Feature | GitHub Native | Codecov/Coveralls |
|---------|---------------|-------------------|
| **Setup** | No signup, no tokens | Requires account + token |
| **Privacy** | Code stays in repo | Uploads to external service |
| **Cost** | Free forever | Free tier limits |
| **Speed** | Native Actions | External API calls |
| **Control** | Full customization | Limited to service features |
| **Maintenance** | No dependency on 3rd party | Service must stay online |

## Advanced Features

### Coverage Trends

Track coverage over time by parsing badge JSON:
```bash
git log --all --oneline .github/badges/coverage.json
```

### Coverage Comparison (PR vs Main)

Add to `.github/workflows/test.yml`:
```bash
# Checkout main branch coverage
git fetch origin main
git show origin/main:.github/badges/coverage.json > main_coverage.json
MAIN_COV=$(jq -r '.message' main_coverage.json | sed 's/%//')

# Compare
echo "Main: ${MAIN_COV}%"
echo "PR: ${COVERAGE}%"
```

### Enforce Coverage in CI

Fail CI if coverage drops:
```yaml
- name: Enforce coverage threshold
  run: |
    if (( $(echo "$COVERAGE < 70" | bc -l) )); then
      echo "::error::Coverage below threshold"
      exit 1
    fi
```

## Migration from Codecov

If migrating from Codecov:

1. ✅ Remove `codecov/codecov-action` from workflows
2. ✅ Delete `.codecov.yml` configuration
3. ✅ Update badge URLs in README
4. ✅ Remove `CODECOV_TOKEN` from GitHub secrets
5. ✅ Run new workflows to generate initial badge

That's it! No external dependencies needed.
