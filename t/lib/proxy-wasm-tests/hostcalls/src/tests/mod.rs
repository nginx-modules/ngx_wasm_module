pub mod echo;

use crate::{types::*, *};
use chrono::{DateTime, Utc};
use log::*;
use proxy_wasm::hostcalls::*;
use proxy_wasm::types::*;

pub(crate) fn test_log_levels(_: &mut TestHttp) {
    trace!("proxy_log trace");
    info!("proxy_log info");
    warn!("proxy_log warn");
    error!("proxy_log error");
    log(LogLevel::Critical, "proxy_log critical").unwrap();
}

pub(crate) fn test_log_current_time(ctx: &mut TestHttp) {
    let now: DateTime<Utc> = ctx.get_current_time().into();
    info!("now: {}", now)
}

pub(crate) fn test_log_request_header(ctx: &mut TestHttp) {
    if let Some(header_name) = ctx.config.get("name") {
        let value = ctx.get_http_request_header(header_name.as_str());
        if value.is_some() {
            info!("request header \"{}: {}\"", header_name, value.unwrap());
        }
    }
}

pub(crate) fn test_log_request_headers(ctx: &mut TestHttp) {
    for (name, value) in ctx.get_http_request_headers() {
        info!("{}: {}", name, value)
    }
}

pub(crate) fn test_log_request_body(ctx: &mut TestHttp) {
    let body = ctx.get_http_request_body(0, 30);
    if let Some(bytes) = body {
        match String::from_utf8(bytes) {
            Ok(s) => info!("request body: {}", s),
            Err(e) => panic!("Invalid UTF-8 sequence: {}", e),
        }
    }
}

pub(crate) fn test_log_response_body(ctx: &mut TestHttp) {
    let max_len = ctx
        .config
        .get("max_len")
        .map(|max| max.parse::<usize>().unwrap())
        .unwrap_or(usize::MAX);

    if let Some(bytes) = ctx.get_http_response_body(0, max_len) {
        match String::from_utf8(bytes) {
            Ok(s) => info!("response body chunk: {:?}", s),
            Err(e) => panic!("Invalid UTF-8 sequence: {}", e),
        }
    }
}

pub(crate) fn test_log_response_header(ctx: &mut TestHttp) {
    if let Some(header_name) = ctx.config.get("name") {
        let value = ctx.get_http_response_header(header_name.as_str());
        if value.is_some() {
            info!("resp header \"{}: {}\"", header_name, value.unwrap());
        }
    }
}

pub(crate) fn test_log_response_headers(ctx: &mut TestHttp) {
    for (name, value) in ctx.get_http_response_headers() {
        info!("resp {}: {}", name, value)
    }
}

pub(crate) fn test_log_request_path(ctx: &mut TestHttp) {
    let path = ctx
        .get_http_request_header(":path")
        .expect("failed to retrieve request path");

    info!("path: {}", path);
}

pub(crate) fn test_log_property(ctx: &(dyn TestContext + 'static)) {
    let name = ctx.get_config("name").expect("expected a name argument");

    match ctx.get_property(name.split('.').collect()) {
        Some(p) => match std::str::from_utf8(&p) {
            Ok(value) => info!("{}: {}", name, value),
            Err(_) => panic!("failed converting {} to UTF-8", name),
        },
        None => info!("property not found: {}", name),
    }
}

pub(crate) fn test_send_status(ctx: &mut TestHttp, status: u32) {
    ctx.send_http_response(status, vec![], None)
}

pub(crate) fn test_send_headers(ctx: &mut TestHttp) {
    ctx.send_http_response(200, vec![("Powered-By", "proxy-wasm")], None)
}

pub(crate) fn test_send_body(ctx: &mut TestHttp) {
    //let path = ctx.get_http_request_header(":path").unwrap();
    ctx.send_http_response(
        200,
        vec![("Content-Length", "0")], // must be overriden to body.len() by host
        Some("Hello world".as_bytes()),
        //Some(format!("Hello world ({})", path).as_bytes()),
    )
}

pub(crate) fn test_send_twice(ctx: &mut TestHttp) {
    ctx.send_http_response(200, vec![], Some("Send once".as_bytes()));
    ctx.send_http_response(201, vec![], Some("Send twice".as_bytes()))
}

pub(crate) fn test_set_special_headers(ctx: &mut TestHttp) {
    ctx.send_http_response(
        200,
        vec![
            ("Server", "proxy-wasm"),
            ("Date", "Wed, 22 Oct 2020 07:28:00 GMT"),
            ("Content-Length", "3"), // No effect (overriden)
            ("Content-Encoding", "gzip"),
            ("Location", "/index.html"),
            ("Refresh", "5; url=http://www.w3.org/index.html"),
            ("Last-Modified", "Tue, 15 Nov 1994 12:45:26 GMT"),
            ("Content-Range", "bytes 21010-47021/47022"),
            ("Accept-Ranges", "bytes"),
            ("WWW-Authenticate", "Basic"),
            ("Expires", "Thu, 01 Dec 1994 16:00:00 GMT"),
            //("ETag", "737060cd8c284d8af7ad3082f209582d"),
            ("E-Tag", "377060cd8c284d8af7ad3082f20958d2"),
            ("Content-Type", "text/plain; charset=UTF-8"),
            ("Cache-Control", "no-cache"),
            ("Link", "</feed>; rel=\"alternate\""),
        ],
        None,
    )
}

pub(crate) fn test_set_headers_escaping(ctx: &mut TestHttp) {
    ctx.send_http_response(
        200,
        vec![
            ("Escape-Colon:", "value"),
            ("Escape-Parenthesis()", "value"),
            ("Escape-Quote\"", "value"),
            ("Escape-Comps<>", "value"),
            ("Escape-Equal=", "value"),
        ],
        None,
    )
}

pub(crate) fn test_set_request_headers(ctx: &mut TestHttp) {
    ctx.set_http_request_headers(vec![("Hello", "world"), ("Welcome", "wasm")]);
}

pub(crate) fn test_set_request_headers_special(ctx: &mut TestHttp) {
    ctx.set_http_request_headers(vec![
        (":path", "/updated"),
        ("Host", "somehost"),
        ("Connection", "closed"),
        ("User-Agent", "Gecko"),
        ("Content-Type", "text/none"),
        ("X-Forwarded-For", "128.168.0.1"),
    ]);
}

pub(crate) fn test_set_request_headers_invalid(ctx: &mut TestHttp) {
    ctx.set_http_request_headers(vec![(":scheme", "https")]);
}

pub(crate) fn test_set_http_request_header(ctx: &mut TestHttp) {
    if let Some(name) = ctx.config.get("name") {
        let value = ctx.config.get("value").expect("missing value");
        ctx.set_http_request_header(name, Some(value));
    } else if let Some(config) = ctx.config.get("value") {
        let (name, value) = config.split_once(':').unwrap();
        if value.is_empty() {
            ctx.set_http_request_header(name, None);
        } else {
            ctx.set_http_request_header(name, Some(value));
        }
    }
}

pub(crate) fn test_add_http_request_header(ctx: &mut TestHttp) {
    if let Some(config) = ctx.config.get("value") {
        let (name, value) = config.split_once(':').unwrap();
        ctx.add_http_request_header(name, value);
    } else if let Some(header) = ctx.get_http_request_header("pwm-add-req-header") {
        let (name, value) = header.split_once('=').unwrap();
        ctx.add_http_request_header(name, value);
    }
}

pub(crate) fn test_add_http_response_header(ctx: &mut TestHttp) {
    if let Some(header) = ctx.config.get("value") {
        let (name, value) = header.split_once(':').unwrap();
        ctx.add_http_response_header(name, value);
    }
}

pub(crate) fn test_set_response_header(ctx: &mut TestHttp) {
    if let Some(header) = ctx.config.get("value") {
        let (name, value) = header.split_once(':').unwrap();
        if value.is_empty() {
            ctx.set_http_response_header(name, None);
        } else {
            ctx.set_http_response_header(name, Some(value));
        }
    }
}

pub(crate) fn test_set_response_headers(ctx: &mut TestHttp) {
    if let Some(headers_str) = ctx.config.get("value") {
        let headers = headers_str
            .split('+')
            .filter_map(|s| s.split_once(':'))
            .collect();

        ctx.set_http_response_headers(headers);
    } else {
        ctx.set_http_response_headers(vec![("Hello", "world"), ("Welcome", "wasm")]);
    }
}

pub(crate) fn test_set_http_request_body(ctx: &mut TestHttp) {
    let body = if let Some(value) = ctx.config.get("value") {
        value.to_string()
    } else {
        "Hello world".into()
    };

    let offset = ctx
        .config
        .get("offset")
        .map_or(0, |v| v.parse::<usize>().unwrap());

    let len = ctx
        .config
        .get("max")
        .map_or(body.len(), |v| v.parse::<usize>().unwrap());

    ctx.set_http_request_body(offset, len, body.as_bytes());
}

pub(crate) fn test_set_response_body(ctx: &mut TestHttp) {
    let key = "set_response_body".to_string();
    if ctx.config.get(&key).is_some() {
        return;
    }

    let mut body = if let Some(value) = ctx.config.get("value") {
        value.to_string()
    } else {
        "Hello world".into()
    };

    if !body.is_empty() {
        body.push('\n');
    }

    let offset = ctx
        .config
        .get("offset")
        .map_or(0, |v| v.parse::<usize>().unwrap());

    let len = ctx
        .config
        .get("max")
        .map_or(body.len(), |v| v.parse::<usize>().unwrap());

    ctx.set_http_response_body(offset, len, body.as_bytes());

    ctx.config.insert(key.clone(), key);
}