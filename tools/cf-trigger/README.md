# Cloudflare Worker trigger for the outage normalizer

GitHub's `schedule` trigger fires rarely for small repositories (on 2026-11-27 — twice a day instead of every
30 minutes). This Worker's Cron Trigger calls `workflow_dispatch` for `.github/workflows/outages.yml` at :05 and :35
every hour. The adapters, the GitHub Pages URLs and the firmware stay unchanged; GitHub's own schedule stays as a
fallback.

Free plan limits are far away: 5 cron triggers per account, 100 000 requests a day; GitHub Actions minutes are free
for public repositories.

## What you need

1. A Cloudflare account (free): <https://dash.cloudflare.com/sign-up>.
2. A GitHub **fine-grained token** (account Settings → Developer settings → Personal access tokens →
   Fine-grained tokens → Generate new token, or <https://github.com/settings/personal-access-tokens/new>):
   - Repository access: **Only select repositories** → `deye-mini-display`;
   - Permissions → Repository permissions → **Actions: Read and write** (Metadata read-only is added automatically);
   - Expiration: up to a year — put a reminder in your calendar to issue a new one before it expires.

Never commit the token or paste it anywhere but the secret prompt below.

## Deploy with wrangler (Node.js 18+)

From this folder (`tools/cf-trigger`):

```
npx wrangler@4 login
npx wrangler@4 deploy
npx wrangler@4 secret put GITHUB_TOKEN
```

- `login` opens the browser to allow wrangler access to your Cloudflare account.
- `secret put` asks for the value — paste the GitHub token there (it is not echoed).

## Or deploy from the dashboard

1. Workers & Pages → **Create** → **Create Worker** (Hello World) → name it `deye-outages-trigger` → **Deploy**.
2. **Edit code**: replace everything with [`worker.js`](worker.js) → **Deploy**.
3. **Settings → Variables and Secrets → Add**: type **Secret**, name `GITHUB_TOKEN`, value — the token.
4. **Settings → Trigger events → Add → Cron Triggers**: `5,35 * * * *`.
5. **Settings → Domains & Routes**: disable the `workers.dev` route (the Worker needs no public URL).

## Check that it works

- In the Worker's **Settings → Trigger events** (or **Logs**) the next runs and their result are listed; the log line
  `workflow_dispatch ok` means GitHub accepted the request.
- On GitHub: `gh run list --workflow outages.yml` — runs every ~30 min with the event `workflow_dispatch`.
- Errors in the log: `401`/`403` — the token expired or lacks "Actions: read and write"; `404` — wrong repository or
  workflow name in `worker.js`.
