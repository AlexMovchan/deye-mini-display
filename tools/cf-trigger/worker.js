// Cloudflare Worker: a reliable "alarm clock" for the outage normalizer.
//
// GitHub's own `schedule` trigger fires rarely for small repositories (twice a day instead of every 30 min),
// so this Worker's Cron Trigger asks GitHub to run .github/workflows/outages.yml via workflow_dispatch.
// Everything else (Python adapters, GitHub Pages URLs, firmware) stays unchanged.
//
// Secret GITHUB_TOKEN: a fine-grained token limited to this repository with "Actions: read and write" only.
// It is set with `wrangler secret put GITHUB_TOKEN` (or in the Cloudflare dashboard) — never in this file.

const REPO = "AlexMovchan/deye-mini-display";
const WORKFLOW = "outages.yml";
const REF = "main";

async function dispatch(env) {
  if (!env.GITHUB_TOKEN) throw new Error("GITHUB_TOKEN secret is not set");
  const r = await fetch(`https://api.github.com/repos/${REPO}/actions/workflows/${WORKFLOW}/dispatches`, {
    method: "POST",
    headers: {
      Authorization: `Bearer ${env.GITHUB_TOKEN}`,
      Accept: "application/vnd.github+json",
      "X-GitHub-Api-Version": "2022-11-28",
      "User-Agent": "deye-mini-display-cf-trigger",
    },
    body: JSON.stringify({ ref: REF }),
  });
  // 204 — accepted. 401/403 — token expired or lacks "Actions: read and write"; 404 — wrong repo/workflow name.
  if (r.status !== 204) {
    const text = (await r.text()).slice(0, 300);
    console.log(`workflow_dispatch failed: ${r.status} ${text}`);
    throw new Error(`workflow_dispatch failed: ${r.status}`);
  }
  console.log("workflow_dispatch ok");
}

export default {
  async scheduled(event, env, ctx) {
    ctx.waitUntil(dispatch(env));
  },
  // No public trigger: anyone could otherwise start the workflow by opening the URL.
  async fetch() {
    return new Response("Not found", { status: 404 });
  },
};
