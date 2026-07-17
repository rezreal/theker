// Populates the version picker and points esp-web-tools at the chosen build.
//
// Everything is same-origin. GitHub's CORS rules block a Pages site from
// fetching a release asset with JS (esp-web-tools#521), so the release workflow
// copies each firmware into this site and regenerates versions.json. That file
// is therefore the source of truth: it can only ever list builds that are
// actually here and flashable.

const versionSelect = document.getElementById("version");
const notesLink = document.getElementById("notes");
const statusEl = document.getElementById("status");
const installer = document.getElementById("installer");
const repoLink = document.getElementById("repo");

function setStatus(message, kind = "") {
  statusEl.textContent = message;
  statusEl.className = `status ${kind}`;
}

function selectRelease(release) {
  installer.manifest = release.manifest;
  installer.setAttribute("manifest", release.manifest);

  if (release.notes) {
    notesLink.href = release.notes;
    notesLink.hidden = false;
  } else {
    notesLink.hidden = true;
  }
}

async function load() {
  let data;
  try {
    const response = await fetch("versions.json", { cache: "no-store" });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    data = await response.json();
  } catch (error) {
    versionSelect.innerHTML = "<option>Unavailable</option>";
    setStatus(`Could not load the firmware list: ${error.message}`, "warn");
    return;
  }

  if (data.repository) {
    repoLink.href = data.repository;
  }

  const releases = data.releases ?? [];
  if (releases.length === 0) {
    versionSelect.innerHTML = "<option>No releases yet</option>";
    setStatus("No firmware has been released yet.", "warn");
    return;
  }

  versionSelect.innerHTML = "";
  for (const release of releases) {
    const option = document.createElement("option");
    option.value = release.version;
    option.textContent = release.date
      ? `${release.version} — ${release.date}`
      : release.version;
    versionSelect.append(option);
  }
  versionSelect.disabled = false;

  versionSelect.addEventListener("change", () => {
    const release = releases.find((r) => r.version === versionSelect.value);
    if (release) {
      selectRelease(release);
      setStatus(`Ready to flash ${release.version}.`);
    }
  });

  selectRelease(releases[0]);
  setStatus(`Ready to flash ${releases[0].version} (latest).`);
}

load();
