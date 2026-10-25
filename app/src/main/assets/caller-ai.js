/* Builds the on-device model prompt for the caller and parses its reply. Shared by the app and tools/chat-test.js. */
(function(root){
  const MAX_HISTORY=30; // a whole call fits, so the prompt stays a growing prefix

  const scenarios=[
    {name:'Doreen Pike',product:'GhostGuard Premium Support',payout:240,trust:32,skeptic:true,
     bio:'You are a 71 year old retired school librarian who lives alone with two cats. You are polite but stubborn.',
     situation:'He left you a voicemail about a virus on your computer. Your grandson warned you that these calls are always scams.',
     opening:'Hello? Your company left me a voicemail about some sort of computer problem. My grandson says these calls are all scams. Who exactly am I speaking with?'},
    {name:'Martin Bell',product:'Refund Recovery Desk',payout:320,trust:43,skeptic:true,
     bio:'You are a 54 year old accountant. You are dry, precise and you love asking for reference numbers.',
     situation:'He says you are owed a refund on an invoice. You have an invoice in front of you but you do not recognize the company on it.',
     opening:'You called about a refund? I have the invoice here, but I don\u2019t recognize your company. Explain.'},
    {name:'Rita Gomez',product:'Prize Verification Bureau',payout:260,trust:48,skeptic:false,
     bio:'You are a 29 year old nurse heading out to a night shift. You are cheerful, distracted and in a hurry.',
     situation:'He says you won a prize and only need to verify it. You would love to win something, but you have about two minutes.',
     opening:'Hello? I got your message. I have about two minutes before work. What is this about?'},
    {name:'Elliot Marsh',product:'GhostGuard Premium Support',payout:380,trust:26,skeptic:true,
     bio:'You are a 40 year old contractor who is sick of spam calls. You are blunt, sarcastic and short tempered.',
     situation:'This company has called you several times this week about your computer. You are already irritated.',
     opening:'You people keep calling me. What do you want this time?'}
  ];

  function system(s){
    return `You are ${s.name}. ${s.bio}
A stranger named George just phoned you claiming to be from "${s.product}". ${s.situation}
Personality: ${s.skeptic?'sharp, suspicious, quick to notice contradictions':'friendly but busy, a little gullible, wants to get off the phone'}.

This is a comedy phone-scam game. You are the target, not the scammer. Stay in character as ${s.name} the whole time.
Talk like a real person on the phone. Reply with 1 to 3 short sentences. No narration, no emojis, never mention being an AI.
React honestly to what George says. Ask questions, poke holes in weak stories, get annoyed by pushiness, rudeness or threats.
If George is friendly, specific and answers your worries, slowly warm up to him. Only agree to pay when you are truly convinced.
Each of George's lines shows your trust in him from 0 to 100 before he spoke.

After your spoken line, write exactly one tag on a new line:
[MOOD +N] or [MOOD -N] with N from 0 to 15, for how George's last message changed your trust.
[HANG UP] if you end the call.
[PAY] only if you agree to pay or hand over what he asked for.`;
  }

  // Qwen3.5 chat format with the thinking block left empty, which keeps replies fast.
  // The prompt only ever grows at the end, so the engine can reuse its memory of the previous turn.
  // Pass the player's new line as the last message in state.messages, with its trust before the line.
  const ASSISTANT='<|im_start|>assistant\n<think>\n\n</think>\n\n';
  function buildPrompt(s,state){
    const turns=state.messages.filter(m=>m.role==='caller'||m.role==='player');
    const start=Math.max(0,turns.length-MAX_HISTORY);
    let out=`<|im_start|>system\n${system(s)}<|im_end|>\n<|im_start|>user\n(Your phone rings and you pick up.)<|im_end|>\n`;
    for(const m of turns.slice(start)){
      if(m.role==='caller')out+=`${ASSISTANT}${m.text}<|im_end|>\n`;
      else out+=`<|im_start|>user\nGeorge: ${m.text}\n(Your trust: ${m.trust??state.trust}/100)<|im_end|>\n`;
    }
    return out+ASSISTANT;
  }

  // Returns {text, mood (number or null), hangup, pay}.
  function parseReply(raw,s){
    let t=String(raw||'').replace(/<think>[\s\S]*?<\/think>/g,'').replace(/<\|im_(start|end)\|>/g,'');
    let mood=null,hangup=false,pay=false;
    const m=t.match(/\[\s*MOOD\s*([+-]?\s*\d+)\s*\]/i);
    if(m)mood=Math.max(-15,Math.min(15,parseInt(m[1].replace(/\s/g,''),10)||0));
    if(/\[\s*HANG\s*UP\s*\]/i.test(t))hangup=true;
    if(/\[\s*PAY\s*\]/i.test(t))pay=true;
    t=t.replace(/\[[^\]]*\]/g,' ');
    // Drop a leading speaker label and stage directions the model sometimes adds.
    t=t.replace(new RegExp('^\\s*(\\*\\*)?('+s.name.replace(/[.*+?^${}()|[\]\\]/g,'\\$&')+'|'+s.name.split(' ')[0]+')(\\*\\*)?\\s*:\\s*','i'),'');
    t=t.replace(/^\s*[(*][^)*]{1,80}[)*]\s*/,'').replace(/\*/g,'').replace(/\s+\n/g,'\n').replace(/[ \t]{2,}/g,' ').trim();
    if(/^["\u201c][^"\u201c\u201d]*["\u201d]$/.test(t))t=t.slice(1,-1).trim();
    t=t.slice(0,600);
    return {text:t,mood,hangup,pay};
  }

  const api={scenarios,buildPrompt,parseReply};
  if(typeof module!=='undefined'&&module.exports)module.exports=api;
  else root.CallerAI=api;
})(typeof window!=='undefined'?window:globalThis);
