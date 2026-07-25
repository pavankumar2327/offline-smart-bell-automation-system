# Smart Bell REST API

All protected endpoints require `Authorization: Bearer <token>` or the `SBSESSION` cookie.

## Authentication

- `POST /api/auth/login` with `{ "username": "...", "password": "..." }`
- `POST /api/auth/logout`

## System

- `GET /api/status`
- `POST /api/device/restart`
- `GET /api/logs`
- `DELETE /api/logs`

## Settings

- `GET /api/settings`
- `PUT /api/settings`

## Profiles

- `GET /api/profiles`
- `PUT /api/profiles`
- `POST /api/profiles/activate` with `{ "id": "regular" }`

## Schedules

- `GET /api/schedules?profile=regular`
- `PUT /api/schedules`

Payload:

```json
{
  "profileId": "regular",
  "entries": [
    {
      "time": "09:00",
      "duration": 3000,
      "repeatCount": 2,
      "repeatInterval": 1000,
      "enabled": true,
      "label": "Morning Bell"
    }
  ]
}
```

## Holidays

- `GET /api/holidays`
- `PUT /api/holidays`

## Manual Bell Control

- `POST /api/bell` with `{ "action": "ring", "duration": 3000 }`
- `POST /api/bell` with `{ "action": "continuous" }`
- `POST /api/bell` with `{ "action": "stop" }`
- `POST /api/bell` with `{ "action": "test", "duration": 1000 }`

## Time Sync

- `POST /api/time/sync-ntp`
- `POST /api/time/sync-browser` with `{ "epoch": 1779085200 }`

